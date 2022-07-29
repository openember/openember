#!/bin/bash

VERSION=1.0.0
AUTHOR=luhuadong
EMAIL=luhuadong@163.com
BUILD_DIR=`pwd`/build
PACKAGE=agloo-${VERSION}.zip
CONF_DIR=/etc/agloo

CheckPermission() {
    echo "Check user permission ..."

    account=`whoami`
    if [ ${account} != "root" ]; then
        echo "${account}, you are NOT the supervisor."
        echo "The root permission is required to run this installer."
        echo "Permission denied."
        exit 1
    fi
}

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

DeployAll() {
    echo "Deploy application ..."
    CheckPermission

    if [ ! -d ${BUILD_DIR} ]; then
        echo "You haven't built agloo, please run again with `--build` option."
        return -1
    fi

    cd ${BUILD_DIR}

    if [ ! -d ${CONF_DIR} ]; then
        echo "Create ${CONF_DIR} ..."
        mkdir ${CONF_DIR}
    fi

    cp ../libs/Log/zlog.conf ${CONF_DIR}
    chmod 777 ${CONF_DIR}/zlog.conf
}

if [ -z $1 ] || [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
    echo "Agloo CLI tool v${VERSION}, ${EMAIL}"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "OPTIONS"
    echo "  -b, --build        Build executable file from sources."
    echo "  -t, --test         Build and run all test cases."
    echo "  -p, --package      Package executable files in a tar ball."
    echo "  -s, --server       Build executable file and run a local web server."
    echo "  -d, --deploy       Deploy agloo apps in host/target."
    echo "  -c, --clean        Delete all build files."
    echo "  -a, --all          Rebuild and pack all executable files"
    echo "  -h, --help         Display this help and exit"
    echo "  -v, --version      Output version information and exit"
    echo ""
    exit 0
elif [ "$1" == "-v" ] || [ "$1" == "--version" ]; then
    echo "Script Version: ${VERSION}"
    echo "Author: ${AUTHOR}"
    echo "E-mail: ${EMAIL}"
    exit 0
elif [ "$1" == "-b" ] || [ "$1" == "--build" ]; then
    RemoveExpiredFiles
    BuildProject
elif [ "$1" == "-t" ] || [ "$1" == "--test" ]; then
    #RemoveExpiredFiles
    BuildTest
elif [ "$1" == "-p" ] || [ "$1" == "--package" ]; then
    #RemoveExpiredFiles
    #BuildProject
    PackageAll
elif [ "$1" == "-s" ] || [ "$1" == "--server" ]; then
    #RemoveExpiredFiles
    #BuildProject
    RunServer
elif [ "$1" == "-d" ] || [ "$1" == "--deploy" ]; then
    #RemoveExpiredFiles
    #BuildProject
    DeployAll
elif [ "$1" == "-c" ] || [ "$1" == "--clean" ]; then
    RemoveExpiredFiles
elif [ "$1" == "-a" ] || [ "$1" == "--all" ]; then
    RemoveExpiredFiles
    BuildProject
    BuildTest
    PackageAll
    #scp ${PACKAGE} board:/tmp/
else 
    echo "Invalid parameters ($1)"
    exit 1
fi

echo "Done!"
exit 0