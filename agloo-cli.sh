#!/bin/bash

VERSION=1.0.0
AUTHOR=luhuadong
BUILD_DIR=`pwd`/build
PACKAGE=agloo-${VERSION}.zip

RemoveExpiredFiles() {
    if [ -d ${BUILD_DIR} ]; then
        echo "Delete build/* ..."
        rm -r ${BUILD_DIR}/*
    fi
}

BuildProject() {
    cd ${BUILD_DIR}
    cmake ..
    make
}

BuildTest() {
    cd ${BUILD_DIR}
    cmake ..
    make
    make test
}

RunServer() {
    cd ${BUILD_DIR}
    ./bin/WebServer
}

PackageAll() {
    echo "Package all bin ..."
    zip -q -r ${PACKAGE} ${BUILD_DIR}/bin
    echo "See -> ${PACKAGE}"
}

if [ -z $1 ] || [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
    echo "Usage: $0 [options]"
    echo ""
    echo "OPTIONS"
    echo "  -b, --build        Build executable file from sources."
    echo "  -t, --test         Build and run all test cases."
    echo "  -p, --package      Package executable files in a tar ball."
    echo "  -s, --server       Build executable file and run a local web server."
    echo "  -d, --delete       Delete all build files."
    echo "  -a, --all          Rebuild and pack all executable files"
    echo "  -h, --help         Display this help and exit"
    echo "  -v, --version      Output version information and exit"
    echo ""
    exit 0
elif [ "$1" == "-v" ] || [ "$1" == "--version" ]; then
    echo "Script Version: ${VERSION}"
    echo "Author: ${AUTHOR}"
    exit 0
elif [ "$1" == "-b" ] || [ "$1" == "--build" ]; then
    RemoveExpiredFiles
    BuildProject
elif [ "$1" == "-t" ] || [ "$1" == "--test" ]; then
    RemoveExpiredFiles
    BuildTest
elif [ "$1" == "-p" ] || [ "$1" == "--package" ]; then
    RemoveExpiredFiles
    BuildProject
    PackageAll
elif [ "$1" == "-s" ] || [ "$1" == "--server" ]; then
    RemoveExpiredFiles
    BuildProject
    RunServer
elif [ "$1" == "-d" ] || [ "$1" == "--delete" ]; then
    RemoveExpiredFiles
elif [ "$1" == "-a" ] || [ "$1" == "--all" ]; then
    RemoveExpiredFiles
    BuildProject
    PackageAll
    #scp ${PACKAGE} board:/tmp/
fi

echo "Done!"
exit 0