android {
  include ($${PWD}/Android/Android.pri)
} else
win32 {
  include ($${PWD}/Windows/Windows.pri)
} else
macx {
  include ($${PWD}/MacOSX/MacOSX.pri)
} else
linux-g++ {
  include ($${PWD}/Linux/Linux.pri)
} else
freebsd {
  include ($${PWD}/FreeBSD/FreeBSD.pri)
} else
solaris {
  include ($${PWD}/Solaris/Solaris.pri)
} else
contains(CONFIG,iphoneos) {
  include ($${PWD}/iPhoneOS/iPhoneOS.pri)
}

contains(DEFINES,SkeletonAPI) {
  include ($${PWD}/Skeleton/Skeleton.pri)
}

SOURCES     += $${PWD}/CaPlatforms.cpp
SOURCES     += $${PWD}/CiosMachine.cpp
