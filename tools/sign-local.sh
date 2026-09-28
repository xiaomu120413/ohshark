#!/usr/bin/env bash
# M0-02 本地签名脚本：为 Debug HAP 生成 OpenHarmony 官方 CA 签名的安装包。
#
# 链路（docs/spikes/external-uri.md 全程排障结论）：
#   1. app 证书：generate-app-cert，由 OpenHarmony Application CA 签发，
#      subject 必须是 'OpenHarmony Application Release'（设备按 DN 匹配信任源）。
#   2. profile 证书：generate-profile-cert，由 Application CA 签发，
#      subject 必须是 'OpenHarmony Application Profile Debug'（设备要求
#      profile 签名者的 issuer == issuer-ca，且 subject 匹配 profile-debug-signing-certificate）。
#   3. profile：hap-sign-tool sign-profile（profileCertFile 为 3 证书链；
#      证书字段用带尾部换行的 PEM 文本——工具与设备都接受）。
#   4. profile 必须含 validity（时间窗口），否则设备判"应用已过期"并限制启动。
#   5. HAP：sign-app，appCertFile 为 certChain。
#   6. 设备信任库：若镜像缺少 Profile CA（如华为定制 OpenHarmony），
#      需将 Profile Debug/Release CA 补入 /system/etc/security/trusted_root_ca.json
#      （overlay 可写；见脚本末尾说明，仅一次性操作）。
#   7. module.json5 须 deliveryWithInstall=false，否则应用会被按需回收。
#
# 用法：
#   bash scripts/sign-local.sh <unsigned.hap> [输出路径] [UDID] [bundleName]
# 材料目录：../local-sign/（工程外，不入库）。
# 需要：DevEco Studio 完整安装（SDK toolchains）、openssl、python。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BASE_DIR="${ROOT}/local-sign"   # 本副本位于 ohshark 仓库 tools/ 下，材料目录即仓库内 local-sign/（不入库）

IN_HAP="${1:-${ROOT}/ohshark-qt-hap/entry/build/default/outputs/default/entry-default-unsigned.hap}"
OUT_HAP="${2:-${BASE_DIR}/entry-signed-local.hap}"
UDID="${3:-$(hdc list targets 2>/dev/null | head -1 | tr -d '[:space:]')}"
BUNDLE_NAME="${4:-com.ohshark.qt}"

TOOLCHAINS='C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/toolchains'
HAP_SIGN_TOOL="${TOOLCHAINS}/lib/hap-sign-tool.jar"
OH_P12="${TOOLCHAINS}/lib/OpenHarmony.p12"
: "${OH_P12_PASS:=123456}"  # OpenHarmony 官方模板默认密码（仅本地开发；可通过环境变量覆盖）

KEYSTORE="${BASE_DIR}/hmwb-local.p12"
KEY_ALIAS='hmwb-local'        # app 密钥
PROFILE_KEY_ALIAS='hmwb-profile'  # profile 密钥
KEY_PASS='hmwb123456'

mkdir -p "${BASE_DIR}"

# ---- 1. app keystore（含 app key + profile key 两个 alias）----
if [ ! -f "${KEYSTORE}" ]; then
  echo "[sign-local] 生成应用密钥对"
  keytool -genkeypair -alias "${KEY_ALIAS}" -keyalg EC -groupname secp256r1 \
    -sigalg SHA256withECDSA -dname 'CN=HMWB Local Dev, O=MarkdownWorkbench, C=CN' \
    -keystore "${KEYSTORE}" -storepass "${KEY_PASS}" -keypass "${KEY_PASS}" \
    -validity 3650 -storetype PKCS12 2>/dev/null
fi
if ! keytool -list -keystore "${KEYSTORE}" -storepass "${KEY_PASS}" -storetype PKCS12 2>/dev/null | grep -q "${PROFILE_KEY_ALIAS}"; then
  echo "[sign-local] 生成 profile 密钥"
  keytool -genkeypair -alias "${PROFILE_KEY_ALIAS}" -keyalg EC -groupname secp256r1 \
    -sigalg SHA256withECDSA -dname 'CN=HMWB Profile Dev, O=MarkdownWorkbench, C=CN' \
    -keystore "${KEYSTORE}" -storepass "${KEY_PASS}" -keypass "${KEY_PASS}" \
    -validity 3650 -storetype PKCS12 2>/dev/null
fi

# ---- 2. 导出官方 CA 证书 ----
if [ ! -f "${BASE_DIR}/app-ca.cer" ]; then
  echo "[sign-local] 导出 OpenHarmony 官方 CA 证书"
  openssl pkcs12 -legacy -in "${OH_P12}" -passin pass:"${OH_P12_PASS}" -nokeys -out "${BASE_DIR}/ca-all.pem" 2>/dev/null
  python - "${BASE_DIR}" <<'PYEOF'
import re, sys
base = sys.argv[1]
pem = open(f'{base}/ca-all.pem', encoding='utf-8').read()
blocks = re.findall(r'(subject=[^\n]*\n.*?-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----)', pem, re.S)
need = {
    'openharmony application ca': 'app-ca.cer',
    'openharmony application root ca': 'root-ca.cer',
}
got = {}
for b in blocks:
    subj = b.split('\n')[0].lower()
    for key in need:
        if key in subj and key not in got:
            cert = re.search(r'-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----', b, re.S).group(0)
            got[key] = cert
for key, cert in got.items():
    open(f'{base}/{need[key]}', 'w').write(cert + '\n')
print('exported:', list(got.keys()))
PYEOF
fi

# ---- 3. app 证书（官方 CommonName 必须匹配信任源 app-signing-cert）----
if [ ! -f "${BASE_DIR}/app-oh.certChain.cer" ]; then
  echo "[sign-local] 签发 app 证书（Application CA）"
  java -jar "${HAP_SIGN_TOOL}" generate-app-cert \
    -keyAlias "${KEY_ALIAS}" -keyPwd "${KEY_PASS}" \
    -issuer 'C=CN, O=OpenHarmony, OU=OpenHarmony Team, CN=OpenHarmony Application CA' \
    -issuerKeyAlias 'openharmony application ca' -issuerKeyPwd "${OH_P12_PASS}" \
    -issuerKeystoreFile "${OH_P12}" -issuerKeystorePwd "${OH_P12_PASS}" \
    -subject 'C=CN, O=OpenHarmony, OU=OpenHarmony Team, CN=OpenHarmony Application Release' \
    -validity 3650 -signAlg SHA256withECDSA \
    -keystoreFile "${KEYSTORE}" -keystorePwd "${KEY_PASS}" \
    -outForm certChain -rootCaCertFile "${BASE_DIR}/root-ca.cer" -subCaCertFile "${BASE_DIR}/app-ca.cer" \
    -outFile "${BASE_DIR}/app-oh.certChain.cer"
fi

# ---- 4. profile 证书（subject 匹配 profile-debug-signing-certificate）----
if [ ! -f "${BASE_DIR}/profile-cert.chain.cer" ]; then
  echo "[sign-local] 签发 profile 证书（Application CA）"
  java -jar "${HAP_SIGN_TOOL}" generate-profile-cert \
    -keyAlias "${PROFILE_KEY_ALIAS}" -keyPwd "${KEY_PASS}" \
    -issuer 'C=CN, O=OpenHarmony, OU=OpenHarmony Team, CN=OpenHarmony Application CA' \
    -issuerKeyAlias 'openharmony application ca' -issuerKeyPwd "${OH_P12_PASS}" \
    -issuerKeystoreFile "${OH_P12}" -issuerKeystorePwd "${OH_P12_PASS}" \
    -subject 'C=CN, O=OpenHarmony, OU=OpenHarmony Team, CN=OpenHarmony Application Profile Debug' \
    -validity 3650 -signAlg SHA256withECDSA \
    -keystoreFile "${KEYSTORE}" -keystorePwd "${KEY_PASS}" \
    -outForm certChain -rootCaCertFile "${BASE_DIR}/root-ca.cer" -subCaCertFile "${BASE_DIR}/app-ca.cer" \
    -outFile "${BASE_DIR}/profile-cert.chain.cer"
fi

# ---- 5. profile JSON（PEM 证书字段 + validity 时间窗口 + debug-info）----
python - "${BASE_DIR}" "${UDID}" "${BUNDLE_NAME}" <<'PYEOF'
import json, sys, uuid
base, udid, bundle = sys.argv[1], sys.argv[2], sys.argv[3]
chain = open(f'{base}/app-oh.certChain.cer', encoding='utf-8').read()
first = chain.split('-----END CERTIFICATE-----')[0] + '-----END CERTIFICATE-----'
cert_pem = first + '\n'   # 尾部换行：工具与设备两侧解析都要求
profile = {
    'version-name': '2.0.0',
    'version-code': 2,
    'uuid': str(uuid.uuid4()),
    'type': 'debug',
    'validity': {'not-before': 1577808000, 'not-after': 2524579200},
    'bundle-info': {
        'developer-id': 'OpenHarmony',
        'development-certificate': cert_pem,
        'bundle-name': bundle,
        'apl': 'normal',
        'app-feature': 'hos_normal_app'
    },
    'debug-info': {'device-ids': [udid], 'device-id-type': 'udid'},
    'issuer': 'pki_internal'
}
with open(f'{base}/profile-local.json', 'w', encoding='utf-8') as f:
    json.dump(profile, f, indent=2, ensure_ascii=False)
print('profile-local.json written')
PYEOF

# ---- 6. 签名 profile 与 HAP ----
echo "[sign-local] sign-profile"
java -jar "${HAP_SIGN_TOOL}" sign-profile \
  -keyAlias "${PROFILE_KEY_ALIAS}" -keyPwd "${KEY_PASS}" -signAlg SHA256withECDSA \
  -mode localSign -profileCertFile "${BASE_DIR}/profile-cert.chain.cer" \
  -inFile "${BASE_DIR}/profile-local.json" \
  -keystoreFile "${KEYSTORE}" -keystorePwd "${KEY_PASS}" \
  -outFile "${BASE_DIR}/profile-local.p7b"

echo "[sign-local] sign-app"
java -jar "${HAP_SIGN_TOOL}" sign-app \
  -mode localSign -keyAlias "${KEY_ALIAS}" -keyPwd "${KEY_PASS}" \
  -signAlg SHA256withECDSA \
  -appCertFile "${BASE_DIR}/app-oh.certChain.cer" \
  -profileFile "${BASE_DIR}/profile-local.p7b" \
  -keystoreFile "${KEYSTORE}" -keystorePwd "${KEY_PASS}" \
  -inFile "${IN_HAP}" -outFile "${OUT_HAP}"

echo "[sign-local] 完成: ${OUT_HAP}"
ls -la "${OUT_HAP}"

# ---- 一次性提示：设备信任库 ----
# 若安装报 'verify signature failed / do not come from trusted root'：
# 将 Profile Debug/Release CA（从 OpenHarmony.p12 导出）追加进设备
# /system/etc/security/trusted_root_ca.json（KEY = subject DN，VALUE = PEM 文本），
# 并以 root 覆盖后重启 accesstoken_service。详见 docs/spikes/external-uri.md 第 5 节。