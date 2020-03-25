#!/bin/bash

# Constants
readonly APP_NAME="Reicast"
readonly APP_BUNDLE_IDENTIFIER="com.reicast.${APP_NAME}"
readonly APP_DEPLOYMENT_TARGET="10.9"
readonly SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
readonly LIBSWIRL_ROOT="${SOURCE_ROOT}/libswirl"
readonly LICENSE_ROOT="${LIBSWIRL_ROOT}/license"
readonly MACOS_SOURCE_ROOT="${D_OSX}"
readonly BUILD_ROOT="${CMAKE_CURRENT_BINARY_DIR}"
readonly BUNDLE="${BUILD_ROOT}/${APP_NAME}.app"
readonly BUNDLE_CONENTS_ROOT="${BUNDLE}/Contents"
readonly BUNDLE_MACOS_ROOT="${BUNDLE_CONENTS_ROOT}/MacOS"
readonly BUNDLE_RESOURCES_ROOT="${BUNDLE_CONENTS_ROOT}/Resources"
readonly INFO_PLIST="${BUNDLE_CONENTS_ROOT}/Info.plist"

echo "--- Building Mac App Bundle ---"
echo "Removing any existing app bundle"
rm -rf ${BUNDLE}
echo "Creating the app bundle directories"
mkdir -p ${BUNDLE_MACOS_ROOT}
mkdir -p ${BUNDLE_RESOURCES_ROOT}
echo "Copying the bundle root directory files"
cp ${MACOS_SOURCE_ROOT}/Info.plist ${INFO_PLIST}
echo "Copying the bundle MacOS directory files"
cp ${BUILD_ROOT}/reicast ${BUNDLE_MACOS_ROOT}/${APP_NAME}
echo "Copying the bundle Resources directory files"
cp ${MACOS_SOURCE_ROOT}/AppIcon.icns ${BUNDLE_RESOURCES_ROOT}
cp ${LICENSE_ROOT}/bsd ${BUNDLE_RESOURCES_ROOT}
cp ${LICENSE_ROOT}/dep_gpl ${BUNDLE_RESOURCES_ROOT}
cp ${LICENSE_ROOT}/gpl ${BUNDLE_RESOURCES_ROOT}
cp ${LICENSE_ROOT}/lgpl ${BUNDLE_RESOURCES_ROOT}
# TODO: Process and include language files
# for cat in $(CATALOGS); do \
#     catname=`basename "$$cat"`; \
#     catname=`echo $$catname|sed -e 's/$(CATOBJEXT)$$//'`; \
#     mkdir -p "${BUNDLE}/Contents/Resources/$$catname/LC_MESSAGES"; \
#     cp "$(top_SOURCE_ROOT)/po/$$cat" "${BUNDLE}/Contents/Resources/$$catname/LC_MESSAGES/lxdream$(INSTOBJEXT)"; \
# done
echo "Updating values in Info.plist"
# TODO: Update Info.plist with new version number automatically
sed -i .bak "s/\$(EXECUTABLE_NAME)/${APP_NAME}/g" ${INFO_PLIST}
sed -i .bak "s/\$(PRODUCT_BUNDLE_IDENTIFIER)/${APP_BUNDLE_IDENTIFIER}/g" ${INFO_PLIST}
sed -i .bak "s/\$(PRODUCT_NAME)/${APP_NAME}/g" ${INFO_PLIST}
sed -i .bak "s/\$(MACOSX_DEPLOYMENT_TARGET)/${APP_DEPLOYMENT_TARGET}/g" ${INFO_PLIST}
rm ${INFO_PLIST}.bak
echo "--- Done ---"
