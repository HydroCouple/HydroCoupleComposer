#############################################################################
# Makefile for building: bin/HydroCoupleComposer
# Generated by qmake (3.0) (Qt 5.7.0)
# Project:  HydroCoupleComposer.pro
# Template: app
# Command: /Users/calebbuahin/Qt5.7.0/5.7/clang_64/bin/qmake -spec macx-clang CONFIG+=x86_64 -o Makefile HydroCoupleComposer.pro
#############################################################################

MAKEFILE      = Makefile

first: release
install: release-install
uninstall: release-uninstall
QMAKE         = /Users/calebbuahin/Qt5.7.0/5.7/clang_64/bin/qmake
DEL_FILE      = rm -f
CHK_DIR_EXISTS= test -d
MKDIR         = mkdir -p
COPY          = cp -f
COPY_FILE     = cp -f
COPY_DIR      = cp -f -R
INSTALL_FILE  = install -m 644 -p
INSTALL_PROGRAM = install -m 755 -p
INSTALL_DIR   = cp -f -R
DEL_FILE      = rm -f
SYMLINK       = ln -f -s
DEL_DIR       = rmdir
MOVE          = mv -f
TAR           = tar -cf
COMPRESS      = gzip -9f
DISTNAME      = HydroCoupleComposer1.0.0
DISTDIR = /Users/calebbuahin/Documents/Projects/HydroCouple/HydroCoupleComposer/build/release/.obj/HydroCoupleComposer1.0.0
SUBTARGETS    =  \
		release \
		debug


release: FORCE
	$(MAKE) -f $(MAKEFILE).Release
release-make_first: FORCE
	$(MAKE) -f $(MAKEFILE).Release 
release-all: FORCE
	$(MAKE) -f $(MAKEFILE).Release all
release-clean: FORCE
	$(MAKE) -f $(MAKEFILE).Release clean
release-distclean: FORCE
	$(MAKE) -f $(MAKEFILE).Release distclean
release-install: FORCE
	$(MAKE) -f $(MAKEFILE).Release install
release-uninstall: FORCE
	$(MAKE) -f $(MAKEFILE).Release uninstall
debug: FORCE
	$(MAKE) -f $(MAKEFILE).Debug
debug-make_first: FORCE
	$(MAKE) -f $(MAKEFILE).Debug 
debug-all: FORCE
	$(MAKE) -f $(MAKEFILE).Debug all
debug-clean: FORCE
	$(MAKE) -f $(MAKEFILE).Debug clean
debug-distclean: FORCE
	$(MAKE) -f $(MAKEFILE).Debug distclean
debug-install: FORCE
	$(MAKE) -f $(MAKEFILE).Debug install
debug-uninstall: FORCE
	$(MAKE) -f $(MAKEFILE).Debug uninstall

Makefile: HydroCoupleComposer.pro ../../../../Qt5.7.0/5.7/clang_64/mkspecs/macx-clang/qmake.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/spec_pre.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/qdevice.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/device_config.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/unix.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/mac.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/macx.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/sanitize.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/gcc-base.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/gcc-base-mac.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/clang.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/clang-mac.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/qconfig.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dcore.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dcore_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dextras.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dextras_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dinput.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dinput_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dlogic.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dlogic_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquick.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquick_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickextras.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickextras_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickinput.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickinput_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickrender.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickrender_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3drender.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3drender_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bluetooth.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bluetooth_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bootstrap_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_charts.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_charts_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_clucene_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_concurrent.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_concurrent_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_core.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_core_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_datavisualization.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_datavisualization_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_dbus.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_dbus_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designer.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designer_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designercomponents_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gamepad.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gamepad_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gui.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gui_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_help.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_help_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_location.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_location_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_macextras.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_macextras_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimedia.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimedia_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimediawidgets.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimediawidgets_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_network.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_network_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_nfc.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_nfc_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_opengl.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_opengl_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_openglextensions.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_openglextensions_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_packetprotocol_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_platformsupport_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_positioning.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_positioning_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_printsupport.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_printsupport_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_purchasing.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_purchasing_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qml.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qml_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmldebug_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmldevtools_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmltest.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmltest_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qtmultimediaquicktools_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quick.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quick_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickcontrols2.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickcontrols2_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickparticles_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quicktemplates2_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickwidgets.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickwidgets_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_script.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_script_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scripttools.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scripttools_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scxml.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scxml_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sensors.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sensors_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialbus.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialbus_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialport.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialport_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sql.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sql_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_svg.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_svg_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_testlib.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_testlib_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uiplugin.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uitools.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uitools_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webchannel.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webchannel_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webengine.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webengine_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecore.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecore_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecoreheaders_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginewidgets.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginewidgets_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_websockets.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_websockets_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webview.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webview_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_widgets.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_widgets_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xml.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xml_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xmlpatterns.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xmlpatterns_private.pri \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt_functions.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt_config.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/macx-clang/qmake.conf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/spec_post.prf \
		.qmake.stash \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exclusive_builds.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/default_pre.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/default_pre.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/resolve_config.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exclusive_builds_post.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/default_post.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/sdk.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/default_post.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/objective_c.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/warn_on.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/resources.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/moc.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/unix/opengl.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/uic.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/unix/thread.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/file_copies.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/rez.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/testcase_targets.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exceptions.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/yacc.prf \
		../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/lex.prf \
		HydroCoupleComposer.pro \
		../../../../Qt5.7.0/5.7/clang_64/lib/QtPrintSupport.framework/QtPrintSupport.prl \
		../../../../Qt5.7.0/5.7/clang_64/lib/QtOpenGL.framework/QtOpenGL.prl \
		../../../../Qt5.7.0/5.7/clang_64/lib/QtWidgets.framework/QtWidgets.prl \
		../../../../Qt5.7.0/5.7/clang_64/lib/QtGui.framework/QtGui.prl \
		../../../../Qt5.7.0/5.7/clang_64/lib/QtConcurrent.framework/QtConcurrent.prl \
		../../../../Qt5.7.0/5.7/clang_64/lib/QtCore.framework/QtCore.prl
	$(QMAKE) -spec macx-clang CONFIG+=x86_64 -o Makefile HydroCoupleComposer.pro
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/spec_pre.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/qdevice.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/device_config.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/unix.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/mac.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/macx.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/sanitize.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/gcc-base.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/gcc-base-mac.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/clang.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/clang-mac.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/qconfig.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dcore.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dcore_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dextras.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dextras_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dinput.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dinput_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dlogic.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dlogic_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquick.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquick_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickextras.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickextras_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickinput.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickinput_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickrender.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickrender_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3drender.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3drender_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bluetooth.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bluetooth_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bootstrap_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_charts.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_charts_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_clucene_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_concurrent.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_concurrent_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_core.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_core_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_datavisualization.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_datavisualization_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_dbus.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_dbus_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designer.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designer_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designercomponents_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gamepad.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gamepad_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gui.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gui_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_help.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_help_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_location.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_location_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_macextras.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_macextras_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimedia.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimedia_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimediawidgets.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimediawidgets_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_network.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_network_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_nfc.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_nfc_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_opengl.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_opengl_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_openglextensions.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_openglextensions_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_packetprotocol_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_platformsupport_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_positioning.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_positioning_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_printsupport.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_printsupport_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_purchasing.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_purchasing_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qml.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qml_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmldebug_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmldevtools_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmltest.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmltest_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qtmultimediaquicktools_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quick.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quick_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickcontrols2.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickcontrols2_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickparticles_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quicktemplates2_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickwidgets.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickwidgets_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_script.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_script_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scripttools.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scripttools_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scxml.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scxml_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sensors.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sensors_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialbus.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialbus_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialport.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialport_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sql.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sql_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_svg.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_svg_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_testlib.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_testlib_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uiplugin.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uitools.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uitools_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webchannel.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webchannel_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webengine.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webengine_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecore.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecore_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecoreheaders_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginewidgets.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginewidgets_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_websockets.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_websockets_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webview.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webview_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_widgets.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_widgets_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xml.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xml_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xmlpatterns.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xmlpatterns_private.pri:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt_functions.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt_config.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/macx-clang/qmake.conf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/spec_post.prf:
.qmake.stash:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exclusive_builds.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/default_pre.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/default_pre.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/resolve_config.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exclusive_builds_post.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/default_post.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/sdk.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/default_post.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/objective_c.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/warn_on.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/resources.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/moc.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/unix/opengl.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/uic.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/unix/thread.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/file_copies.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/rez.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/testcase_targets.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exceptions.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/yacc.prf:
../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/lex.prf:
HydroCoupleComposer.pro:
../../../../Qt5.7.0/5.7/clang_64/lib/QtPrintSupport.framework/QtPrintSupport.prl:
../../../../Qt5.7.0/5.7/clang_64/lib/QtOpenGL.framework/QtOpenGL.prl:
../../../../Qt5.7.0/5.7/clang_64/lib/QtWidgets.framework/QtWidgets.prl:
../../../../Qt5.7.0/5.7/clang_64/lib/QtGui.framework/QtGui.prl:
../../../../Qt5.7.0/5.7/clang_64/lib/QtConcurrent.framework/QtConcurrent.prl:
../../../../Qt5.7.0/5.7/clang_64/lib/QtCore.framework/QtCore.prl:
qmake: FORCE
	@$(QMAKE) -spec macx-clang CONFIG+=x86_64 -o Makefile HydroCoupleComposer.pro

qmake_all: FORCE

make_first: release-make_first debug-make_first  FORCE
all: release-all debug-all  FORCE
clean: release-clean debug-clean  FORCE
distclean: release-distclean debug-distclean  FORCE
	-$(DEL_FILE) Makefile
	-$(DEL_FILE) .qmake.stash

release-mocclean:
	$(MAKE) -f $(MAKEFILE).Release mocclean
debug-mocclean:
	$(MAKE) -f $(MAKEFILE).Debug mocclean
mocclean: release-mocclean debug-mocclean

release-mocables:
	$(MAKE) -f $(MAKEFILE).Release mocables
debug-mocables:
	$(MAKE) -f $(MAKEFILE).Debug mocables
mocables: release-mocables debug-mocables

check: first

benchmark: first
FORCE:

dist: distdir FORCE
	(cd `dirname $(DISTDIR)` && $(TAR) $(DISTNAME).tar $(DISTNAME) && $(COMPRESS) $(DISTNAME).tar) && $(MOVE) `dirname $(DISTDIR)`/$(DISTNAME).tar.gz . && $(DEL_FILE) -r $(DISTDIR)

distdir: release-distdir debug-distdir FORCE
	@test -d $(DISTDIR) || mkdir -p $(DISTDIR)
	$(COPY_FILE) --parents ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/spec_pre.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/qdevice.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/device_config.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/unix.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/mac.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/macx.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/sanitize.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/gcc-base.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/gcc-base-mac.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/clang.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/common/clang-mac.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/qconfig.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dcore.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dcore_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dextras.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dextras_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dinput.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dinput_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dlogic.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dlogic_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquick.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquick_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickextras.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickextras_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickinput.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickinput_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickrender.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3dquickrender_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3drender.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_3drender_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bluetooth.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bluetooth_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_bootstrap_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_charts.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_charts_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_clucene_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_concurrent.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_concurrent_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_core.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_core_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_datavisualization.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_datavisualization_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_dbus.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_dbus_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designer.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designer_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_designercomponents_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gamepad.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gamepad_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gui.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_gui_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_help.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_help_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_location.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_location_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_macextras.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_macextras_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimedia.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimedia_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimediawidgets.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_multimediawidgets_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_network.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_network_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_nfc.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_nfc_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_opengl.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_opengl_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_openglextensions.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_openglextensions_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_packetprotocol_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_platformsupport_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_positioning.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_positioning_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_printsupport.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_printsupport_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_purchasing.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_purchasing_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qml.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qml_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmldebug_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmldevtools_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmltest.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qmltest_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_qtmultimediaquicktools_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quick.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quick_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickcontrols2.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickcontrols2_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickparticles_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quicktemplates2_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickwidgets.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_quickwidgets_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_script.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_script_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scripttools.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scripttools_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scxml.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_scxml_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sensors.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sensors_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialbus.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialbus_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialport.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_serialport_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sql.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_sql_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_svg.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_svg_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_testlib.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_testlib_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uiplugin.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uitools.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_uitools_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webchannel.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webchannel_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webengine.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webengine_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecore.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecore_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginecoreheaders_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginewidgets.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webenginewidgets_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_websockets.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_websockets_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webview.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_webview_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_widgets.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_widgets_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xml.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xml_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xmlpatterns.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/modules/qt_lib_xmlpatterns_private.pri ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt_functions.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt_config.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/macx-clang/qmake.conf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/spec_post.prf .qmake.stash ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exclusive_builds.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/default_pre.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/default_pre.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/resolve_config.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exclusive_builds_post.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/default_post.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/sdk.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/default_post.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/objective_c.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/warn_on.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/qt.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/resources.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/moc.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/unix/opengl.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/uic.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/unix/thread.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/file_copies.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/mac/rez.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/testcase_targets.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/exceptions.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/yacc.prf ../../../../Qt5.7.0/5.7/clang_64/mkspecs/features/lex.prf HydroCoupleComposer.pro $(DISTDIR)/

release-distdir: FORCE
	$(MAKE) -e -f $(MAKEFILE).Release distdir DISTDIR=$(DISTDIR)/

debug-distdir: FORCE
	$(MAKE) -e -f $(MAKEFILE).Debug distdir DISTDIR=$(DISTDIR)/

$(MAKEFILE).Release: Makefile
$(MAKEFILE).Debug: Makefile
