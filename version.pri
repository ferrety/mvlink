# Keep source archives and Docker builds independent of Git metadata.
VERSION = $$cat($$PWD/version.txt)
isEmpty(VERSION): error("version.txt must contain the application version")
!contains(VERSION, ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$): error("version.txt must contain MAJOR.MINOR.PATCH")
DEFINES += MVLINK_VERSION=\\\"$$VERSION\\\"
# Regenerate the Makefile when the release version changes.
QMAKE_INTERNAL_INCLUDED_FILES += $$PWD/version.txt
