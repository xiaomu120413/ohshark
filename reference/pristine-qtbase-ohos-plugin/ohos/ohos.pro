TARGET = qohos
PLUGIN_TYPE = platforms
CONFIG += exceptions

QT += \
    core-private \
    graphics_support-private \
    gui-private \
    eventdispatcher_support-private \
    fontdatabase_support-private \
    egl_support-private \
    theme_support-private

OTHER_FILES += $$PWD/ohos.json

HEADERS += \
    $$PWD/accessibility/qohosaccessibilityarkuihelpers.h \
    $$PWD/accessibility/qohosaccessibilityeventhandler.h \
    $$PWD/accessibility/qohosaccessibilityevents.h \
    $$PWD/accessibility/qohosaccessibilitymenuactions.h \
    $$PWD/accessibility/qohosaccessibilitynodehelpers.h \
    $$PWD/accessibility/qohosaccessibilitytree.h \
    $$PWD/accessibility/qohosaccessibilitytreeeventsconsumercommand.h \
    $$PWD/accessibility/qohosaccessibilityprovider.h \
    $$PWD/accessibility/qohosaccessibilitysafeeventcopysupplier.h \
    $$PWD/accessibility/qohosaccessibilitytabbars.h \
    $$PWD/accessibility/qohosaccessibilitytablemodelchangeeventshandler.h \
    $$PWD/accessibility/qohosaccessibleeventsconsumer.h \
    $$PWD/accessibility/qohosaccessibleinterfaceschildrentracker.h \
    $$PWD/accessibility/qohosaccessiblewidgetinterfacesregistry.h \
    $$PWD/accessibility/qohosaccessiblewrappers.h \
    $$PWD/accessibility/qohosplatformaccessibility.h \
    $$PWD/accessibility/qohossafeqaccessibleinterfacewrapper.h \
    $$PWD/qarkui/input.h \
    $$PWD/qarkui/displaymanager.h \
    $$PWD/qarkui/qarkuiutils.h \
    $$PWD/qarkui/qnativenodeapi.h \
    $$PWD/qarkui/qembeddedwindownode.h \
    $$PWD/qarkui/qohosdragaction.h \
    $$PWD/qarkui/qqtembeddedwindownode.h \
    $$PWD/qarkui/qxcomponentregistry.h \
    $$PWD/qarkui/vsync.h \
    $$PWD/qarkui/window.h \
    $$PWD/qarkui/window_manager.h \
    $$PWD/qohosinternalwindowid_p.h \
    $$PWD/qohosapplicationstatetracker.h \
    $$PWD/qohosapppermissions_p.h \
    $$PWD/qohosbigdataeventlogging.h \
    $$PWD/qohosclipboardobject.h \
    $$PWD/qohosdeadlockprotector.h \
    $$PWD/qohosdisplayinfo.h \
    $$PWD/qohoseglplatformcontext.h \
    $$PWD/qohosenums.h \
    $$PWD/qohoseventdispatcher.h \
    $$PWD/qohosfloatingwindow.h \
    $$PWD/qohosforeignwindow.h \
    $$PWD/qohosimageformat.h \
    $$PWD/qohosinputcontext.h \
    $$PWD/qohosinputmethodproxy.h \
    $$PWD/qohosinputmethodeventhandler.h \
    $$PWD/qohosjsenv_p.h \
    $$PWD/qohosjsmain.h \
    $$PWD/qohosjsutils.h \
    $$PWD/qohoskeyevent.h \
    $$PWD/qohoskeyeventconverthelpers.h \
    $$PWD/qohoskeymodifiers.h \
    $$PWD/qohosmimedata.h \
    $$PWD/qohosmtblockingcallsgateway_p.h \
    $$PWD/qohosnativenodekeyevent.h \
    $$PWD/qohosnouichildprocess.h \
    $$PWD/qohospixelmapconversions.h \
    $$PWD/qohosplatformbackingstore.h \
    $$PWD/qohosplatformbackingstoregl.h \
    $$PWD/qohosplatformclipboard.h \
    $$PWD/qohosplatformcursor.h \
    $$PWD/qohosplatformdialoghelper.h \
    $$PWD/qohosplatformdrag.h \
    $$PWD/qohosplatformintegration.h \
    $$PWD/qohosplatformnativeinterface.h \
    $$PWD/qohosplatformoffscreensurface.h \
    $$PWD/qohosplatformscreen.h \
    $$PWD/qohosplatformservices.h \
    $$PWD/qohosplatformtheme.h \
    $$PWD/qohosplatformwindow.h \
    $$PWD/qohosplugincore.h \
    $$PWD/qohospointerstyle.h \
    $$PWD/qohosqabilityinstancesmanager.h \
    $$PWD/qohosqpafunctionsimpl.h \
    $$PWD/qohossharekit.h \
    $$PWD/qohosruntimedevicetypeandmode.h \
    $$PWD/qohosscreenmanager.h \
    $$PWD/qohossettings.h \
    $$PWD/qohossinglethreadexecutor.h \
    $$PWD/qohosstatusbarmenu.h \
    $$PWD/qohossystemlocale.h \
    $$PWD/qohossystemtrayicon.h \
    $$PWD/qohosudmf.h \
    $$PWD/qohosudmfconversions.h \
    $$PWD/qohosudsobject.h \
    $$PWD/qohosuiextensionplatformwindow.h \
    $$PWD/qohosutils.h \
    $$PWD/qohoswatchdog.h \
    $$PWD/qohoswindowmanager.h \
    $$PWD/qohosxcomponentkeyevent.h \
    $$PWD/render/qnativenode.h \
    $$PWD/render/qohosarkuinativegestureshandler.h \
    $$PWD/render/qohosbatchingrequestshandler.h \
    $$PWD/render/qohosdrageventutils.h \
    $$PWD/render/qohosegl.h \
    $$PWD/render/qohoshovereventsgenerator.h \
    $$PWD/render/qohosjswindowregistry.h \
    $$PWD/render/qohosnativeaxiseventhandler.h \
    $$PWD/render/qohosnativedrageventshandler.h \
    $$PWD/render/qohosnativegestureshandler.h \
    $$PWD/render/qohosnativekeyeventshandler.h \
    $$PWD/render/qohosnativemouseeventshandler.h \
    $$PWD/render/qohosnativexcomponentinputhandler.h \
    $$PWD/render/qohossurface.h \
    $$PWD/render/qohosview.h \
    $$PWD/render/qohoswindowproxy.h \
    $$PWD/render/qohoswindowproxydatafactory.h \
    $$PWD/render/qwindowproxyregistry.h \
    $$PWD/render/qxcomponent.h

SOURCES += \
    $$PWD/accessibility/qohosaccessibilityarkuihelpers.cpp \
    $$PWD/accessibility/qohosaccessibilityeventhandler.cpp \
    $$PWD/accessibility/qohosaccessibilityevents.cpp \
    $$PWD/accessibility/qohosaccessibilitymenuactions.cpp \
    $$PWD/accessibility/qohosaccessibilitynodehelpers.cpp \
    $$PWD/accessibility/qohosaccessibilitytree.cpp \
    $$PWD/accessibility/qohosaccessibilitytreeeventsconsumercommand.cpp \
    $$PWD/accessibility/qohosaccessibilityprovider.cpp \
    $$PWD/accessibility/qohosaccessibilitysafeeventcopysupplier.cpp \
    $$PWD/accessibility/qohosaccessibilitytabbars.cpp \
    $$PWD/accessibility/qohosaccessibilitytablemodelchangeeventshandler.cpp \
    $$PWD/accessibility/qohosaccessibleeventsconsumer.cpp \
    $$PWD/accessibility/qohosaccessibleinterfaceschildrentracker.cpp \
    $$PWD/accessibility/qohosaccessiblewidgetinterfacesregistry.cpp \
    $$PWD/accessibility/qohosaccessiblewrappers.cpp \
    $$PWD/accessibility/qohosplatformaccessibility.cpp \
    $$PWD/accessibility/qohossafeqaccessibleinterfacewrapper.cpp \
    $$PWD/qarkui/input.cpp \
    $$PWD/qarkui/displaymanager.cpp \
    $$PWD/qarkui/qnativenodeapi.cpp \
    $$PWD/qarkui/qembeddedwindownode.cpp \
    $$PWD/qarkui/qohosdragaction.cpp \
    $$PWD/qarkui/qqtembeddedwindownode.cpp \
    $$PWD/qarkui/qxcomponentregistry.cpp \
    $$PWD/qarkui/vsync.cpp \
    $$PWD/qarkui/window.cpp \
    $$PWD/qarkui/window_manager.cpp \
    $$PWD/qohosinternalwindowid.cpp \
    $$PWD/qohosapplicationstatetracker.cpp \
    $$PWD/qohosapppermissions.cpp \
    $$PWD/qohosbigdataeventlogging.cpp \
    $$PWD/qohosclipboardobject.cpp \
    $$PWD/qohosdeadlockprotector.cpp \
    $$PWD/qohosdisplayinfo.cpp \
    $$PWD/qohoseglplatformcontext.cpp \
    $$PWD/qohoseventdispatcher.cpp \
    $$PWD/qohosfloatingwindow.cpp \
    $$PWD/qohosforeignwindow.cpp \
    $$PWD/qohosimageformat.cpp \
    $$PWD/qohosinputcontext.cpp \
    $$PWD/qohosinputmethodproxy.cpp \
    $$PWD/qohosinputmethodeventhandler.cpp \
    $$PWD/qohosjsmain.cpp \
    $$PWD/qohosjsutils.cpp \
    $$PWD/qohoskeyevent.cpp \
    $$PWD/qohoskeyeventconverthelpers.cpp \
    $$PWD/qohoskeymodifiers.cpp \
    $$PWD/qohosmimedata.cpp \
    $$PWD/qohosnativenodekeyevent.cpp \
    $$PWD/qohosnouichildprocess.cpp \
    $$PWD/qohospixelmapconversions.cpp \
    $$PWD/qohosplatformbackingstore.cpp \
    $$PWD/qohosplatformbackingstoregl.cpp \
    $$PWD/qohosplatformclipboard.cpp \
    $$PWD/qohosplatformcursor.cpp \
    $$PWD/qohosplatformdialoghelper.cpp \
    $$PWD/qohosplatformdrag.cpp \
    $$PWD/qohosplatformfiledialoghelper.cpp \
    $$PWD/qohosplatformintegration.cpp \
    $$PWD/qohosplatformnativeinterface.cpp \
    $$PWD/qohosplatformoffscreensurface.cpp \
    $$PWD/qohosplatformplugin.cpp \
    $$PWD/qohosplatformscreen.cpp \
    $$PWD/qohosplatformservices.cpp \
    $$PWD/qohosplatformtheme.cpp \
    $$PWD/qohosplatformwindow.cpp \
    $$PWD/qohosplugincore.cpp \
    $$PWD/qohosqabilityinstancesmanager.cpp \
    $$PWD/qohosqpafunctionsimpl.cpp \
    $$PWD/qohossharekit.cpp \
    $$PWD/qohosruntimedevicetypeandmode.cpp \
    $$PWD/qohosscreenmanager.cpp \
    $$PWD/qohossettings.cpp \
    $$PWD/qohossinglethreadexecutor.cpp \
    $$PWD/qohosstatusbarmenu.cpp \
    $$PWD/qohossystemlocale.cpp \
    $$PWD/qohossystemtrayicon.cpp \
    $$PWD/qohosudmf.cpp \
    $$PWD/qohosudmfconversions.cpp \
    $$PWD/qohosudsobject.cpp \
    $$PWD/qohosuiextensionplatformwindow.cpp \
    $$PWD/qohosutils.cpp \
    $$PWD/qohoswatchdog.cpp \
    $$PWD/qohoswindowmanager.cpp \
    $$PWD/qohosxcomponentkeyevent.cpp \
    $$PWD/render/qnativenode.cpp \
    $$PWD/render/qohosarkuinativegestureshandler.cpp \
    $$PWD/render/qohosdrageventutils.cpp \
    $$PWD/render/qohosegl.cpp \
    $$PWD/render/qohoshovereventsgenerator.cpp \
    $$PWD/render/qohosjswindowregistry.cpp \
    $$PWD/render/qohosnativeaxiseventhandler.cpp \
    $$PWD/render/qohosnativedrageventshandler.cpp \
    $$PWD/render/qohosnativegestureshandler.cpp \
    $$PWD/render/qohosnativekeyeventshandler.cpp \
    $$PWD/render/qohosnativemouseeventshandler.cpp \
    $$PWD/render/qohosnativexcomponentinputhandler.cpp \
    $$PWD/render/qohossurface.cpp \
    $$PWD/render/qohosview.cpp \
    $$PWD/render/qohoswindowproxy.cpp \
    $$PWD/render/qohoswindowproxydatafactory.cpp \
    $$PWD/render/qwindowproxyregistry.cpp \
    $$PWD/render/qxcomponent.cpp

DEFINES += \
    QT_NO_CAST_FROM_ASCII \
    QT_NO_DYNAMIC_CAST

PLUGIN_CLASS_NAME = QOhosPlatformIntegrationPlugin
!equals(TARGET, $$QT_DEFAULT_QPA_PLUGIN): PLUGIN_EXTENDS = -

LIBS += \
    -lace_napi.z \
    -lace_ndk.z \
    -lhiappevent_ndk.z \
    -lhilog_ndk.z \
    -lEGL \
    -lnative_display_manager \
    -lnative_vsync \
    -lnative_window \
    -lnative_window_manager \
    -lohfileshare \
    -lohfileuri \
    -lohhicollie \
    -lohinput \
    -lpasteboard \
    -lpixelmap \
    -lqos \
    -ludmf \
    -lohinputmethod

include($$PWD/../../../3rdparty/node-addon-api.pri)

load(qt_plugin)
