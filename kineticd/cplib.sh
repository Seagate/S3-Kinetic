[ ! -d "$1" ] && mkdir -p "$1"
cp *.a $1
cp kmem/*.a  $1
cp ./K_api/Kinetic_Security/api/libksapi.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libgmock.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libgtest.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libcrypto.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libgflags.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libgflags_nothreads.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libglog.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libprotobuf.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libprotoc.a $1
cp ~/uboot-linux/buildroot-2012.11-2015_T1.0/output/staging/usr/lib/libssl.a $1
cp  lldp/*.a  $1
cp smrdb/*.a $1
cp ha_zac_cmds/*.a $1
cp K_api/tcg_api/transports/*.a $1
cp K_api/Kinetic_Security/external-libs/PBKDFlib/*.a $1
cp K_api/tcg_api/api/*.a $1
cp vendor/src/kinetic_cpp_client/*.a $1

