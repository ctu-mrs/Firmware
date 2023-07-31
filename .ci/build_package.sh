#!/bin/bash

set -e

trap 'last_command=$current_command; current_command=$BASH_COMMAND' DEBUG
trap 'echo "$0: \"${last_command}\" command failed with exit code $?"' ERR

# get the path to this script
MY_PATH=`dirname "$0"`
MY_PATH=`( cd "$MY_PATH" && pwd )`

ARTIFACTS_FOLDER=$1

PACKAGE_PATH=$MY_PATH/..

rm -rf /tmp/workspace || echo ""
rm -rf /tmp/px4 || echo ""

## | ----------------------- Install ROS ---------------------- |

$GITHUB_WORKSPACE/.ci_scripts/package_build/install_ros.sh

## | ----------------------- add MRS PPA ---------------------- |

$GITHUB_WORKSPACE/.ci_scripts/package_build/add_ctu_mrs_unstable_ppa.sh

## | ------------------- checkout submodules ------------------ |

git submodule update --init --recursive

## | ------------------ install dependencies ------------------ |

rosdep install -y -v --rosdistro=noetic --from-paths ./

sudo apt-get -y install ros-noetic-catkin python3-catkin-tools

# PX4-specific dependency
sudo pip3 install -u kconfiglib
# $GITHUB_WORKSPACE/Tools/setup/ubuntu.sh --no-nuttx --no-sim-tool

## | ---------------- prepare catkin workspace ---------------- |

WORKSPACE_PATH=/tmp/workspace

mkdir -p $WORKSPACE_PATH/src
cd $WORKSPACE_PATH/

source /opt/ros/noetic/setup.bash

catkin init
catkin config --profile release --cmake-args -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_FLAGS='-std=c++17'
catkin profile set release
catkin config --install

cd src
ln -s $PACKAGE_PATH px4

## | ------------------------ build px4 ----------------------- |

catkin build

## | -------- extract build artefacts into deb package -------- |

TMP_PATH=/tmp/px4

mkdir -p $TMP_PATH/package/DEBIAN
mkdir -p $TMP_PATH/package/opt/ros/noetic/share

cp -r $WORKSPACE_PATH/install/share/px4 $TMP_PATH/package/opt/ros/noetic/share

# extract package version
VERSION=$(cat $PACKAGE_PATH/package.xml | grep '<version>' | sed -e 's/\s*<\/*version>//g')
echo "$0: Detected version $VERSION"

CPU_ARCH=$(uname -m)
if [[ "$CPU_ARCH" == "x86_64" ]]; then
  echo "$0: detected amd64 architecture"
  ARCH="amd64"
else
  echo "$0: architecture not detected, assuming arm64"
  ARCH="arm64"
fi

echo "Package: ros-noetic-px4
Version: $VERSION
Architecture: $ARCH
Maintainer: Tomas Baca <tomas.baca@fel.cvut.cz>
Description: PX4" > $TMP_PATH/package/DEBIAN/control

cd $TMP_PATH

sudo apt-get -y install dpkg-dev

dpkg-deb --build --root-owner-group package
dpkg-name package.deb

mv $TMP_PATH/*.deb $ARTIFACTS_FOLDER
