# Install script for directory: /home/user/Cataclysm-DDA/data

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/llvm-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/share/cataclysm-dda/font;/usr/local/share/cataclysm-dda/json;/usr/local/share/cataclysm-dda/mods;/usr/local/share/cataclysm-dda/names;/usr/local/share/cataclysm-dda/raw;/usr/local/share/cataclysm-dda/motd;/usr/local/share/cataclysm-dda/credits;/usr/local/share/cataclysm-dda/title;/usr/local/share/cataclysm-dda/core;/usr/local/share/cataclysm-dda/screenshots;/usr/local/share/cataclysm-dda/xdg;/usr/local/share/cataclysm-dda/help")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/share/cataclysm-dda" TYPE DIRECTORY FILES
    "/home/user/Cataclysm-DDA/data/font"
    "/home/user/Cataclysm-DDA/data/json"
    "/home/user/Cataclysm-DDA/data/mods"
    "/home/user/Cataclysm-DDA/data/names"
    "/home/user/Cataclysm-DDA/data/raw"
    "/home/user/Cataclysm-DDA/data/motd"
    "/home/user/Cataclysm-DDA/data/credits"
    "/home/user/Cataclysm-DDA/data/title"
    "/home/user/Cataclysm-DDA/data/core"
    "/home/user/Cataclysm-DDA/data/screenshots"
    "/home/user/Cataclysm-DDA/data/xdg"
    "/home/user/Cataclysm-DDA/data/help"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/share/cataclysm-dda/changelog.txt;/usr/local/share/cataclysm-dda/cataicon.ico;/usr/local/share/cataclysm-dda/fontdata.json")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/share/cataclysm-dda" TYPE FILE FILES
    "/home/user/Cataclysm-DDA/data/changelog.txt"
    "/home/user/Cataclysm-DDA/data/cataicon.ico"
    "/home/user/Cataclysm-DDA/data/fontdata.json"
    )
endif()

