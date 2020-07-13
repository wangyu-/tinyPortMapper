cc_cross=/home/wangyu/Desktop/arm-2014.05/bin/arm-none-linux-gnueabi-g++
cc_local=g++
#cc_mips34kc=/toolchains/OpenWrt-SDK-ar71xx-for-linux-x86_64-gcc-4.8-linaro_uClibc-0.9.33.2/staging_dir/toolchain-mips_34kc_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mips-openwrt-linux-g++
cc_mips24kc_be=/toolchains/lede-sdk-17.01.2-ar71xx-generic_gcc-5.4.0_musl-1.1.16.Linux-x86_64/staging_dir/toolchain-mips_24kc_gcc-5.4.0_musl-1.1.16/bin/mips-openwrt-linux-musl-g++
cc_mips24kc_le=/toolchains/lede-sdk-17.01.2-ramips-mt7621_gcc-5.4.0_musl-1.1.16.Linux-x86_64/staging_dir/toolchain-mipsel_24kc_gcc-5.4.0_musl-1.1.16/bin/mipsel-openwrt-linux-musl-g++
#cc_arm= /toolchains/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-g++ -march=armv6 -marm 
cc_arm= /toolchains/arm-2014.05/bin/arm-none-linux-gnueabi-g++
cc_mingw_cross=x86_64-w64-mingw32-g++-posix
#cc_bcm2708=/home/wangyu/raspberry/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-g++ 

FLAGS= -std=c++11 -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-missing-field-initializers

SOURCES=main.cpp log.cpp common.cpp fd_manager.cpp my_ev.cpp -isystem libev
#SOURCES=new_main.cpp my_ev.cpp -Ilibev

NAME=tinymapper
TARGETS=amd64 arm mips24kc_be x86  mips24kc_le

TAR=${NAME}_binaries.tar.gz `echo ${TARGETS}|sed -r 's/([^ ]+)/tinymapper_\1/g'` version.txt

# targets for nativei (non-cross) compile 
all:git_version
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. ${SOURCES} ${FLAGS} -lrt -ggdb -static -O2

freebsd:git_version
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. ${SOURCES} ${FLAGS} -lrt -ggdb -static -O2

mingw:git_version
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. ${SOURCES} ${FLAGS}  -ggdb -static -O2 -lws2_32

mingw_wepoll:git_version    #to compile you need a pacthed version of libev with wepoll backend
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. main.cpp log.cpp common.cpp fd_manager.cpp ${FLAGS}  -ggdb -static -O2   -DNO_LIBEV_EMBED -D_WIN32 -lev -lws2_32 

mac:git_version
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. ${SOURCES} ${FLAGS}  -ggdb -O2

cygwin:git_version
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. ${SOURCES} ${FLAGS} -lrt -ggdb -static -O2 -D_GNU_SOURCE

#targes for general cross compile

cross:git_version
	${cc_cross}   -o ${NAME}_cross    -I. ${SOURCES} ${FLAGS} -lrt -O2

cross2:git_version
	${cc_cross}   -o ${NAME}_cross    -I. ${SOURCES} ${FLAGS} -lrt -static -lgcc_eh -O2

cross3:git_version
	${cc_cross}   -o ${NAME}_cross    -I. ${SOURCES} ${FLAGS} -lrt -static -O2

#targets only for debug purpose
fast: git_version
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. ${SOURCES} ${FLAGS} -lrt -ggdb
debug: git_version
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. ${SOURCES} ${FLAGS} -lrt -Wformat-nonliteral -D MY_DEBUG 
debug2: git_version
	rm -f ${NAME}
	${cc_local}   -o ${NAME}          -I. ${SOURCES} ${FLAGS} -lrt -Wformat-nonliteral -ggdb

#targets only for cross compile windows targets on linux 

mingw_cross:git_version   #to build this and the below one you need 'mingw-w64' installed (the cross compile version on linux)
	rm -f ${NAME}
	${cc_mingw_cross}   -o ${NAME}.exe          -I. ${SOURCES} ${FLAGS}  -ggdb -static -O2 -lws2_32

mingw_cross_wepoll:git_version    #to compile you need a pacthed version of libev with wepoll backend
	rm -f ${NAME}
	${cc_mingw_cross}   -o ${NAME}_wepoll.exe       -I. main.cpp log.cpp common.cpp fd_manager.cpp ${FLAGS}  -ggdb -static -O2   -DNO_LIBEV_EMBED -D_WIN32 -lev -lws2_32

#targets only for 'make release'

mips24kc_be: git_version
	${cc_mips24kc_be}  -o ${NAME}_$@   -I. ${SOURCES} ${FLAGS} -lrt -lgcc_eh -static -O2

mips24kc_le: git_version
	${cc_mips24kc_le}  -o ${NAME}_$@   -I. ${SOURCES} ${FLAGS} -lrt -lgcc_eh -static -O2

amd64:git_version
	${cc_local}   -o ${NAME}_$@    -I. ${SOURCES} ${FLAGS} -lrt -static -O2

x86:git_version          #to build this you need 'g++-multilib' installed 
	${cc_local}   -o ${NAME}_$@      -I. ${SOURCES} ${FLAGS} -lrt -static -O2 -m32
arm:git_version
	${cc_arm}   -o ${NAME}_$@      -I. ${SOURCES} ${FLAGS} -lrt -static -O2 -ggdb


release: ${TARGETS}
	cp git_version.h version.txt
	tar -zcvf ${TAR}

clean:	
	rm -f ${TAR}
	rm -f tinymapper tinymapper_cross
	rm -f git_version.h

git_version:
	    echo "const char *gitversion = \"$(shell git rev-parse HEAD)\";" > git_version.h
