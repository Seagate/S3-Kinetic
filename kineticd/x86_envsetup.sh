# Source this file to set up the appropriate environment variables for
# x86 build, like this:
#
#       source x86_envsetup.sh

# Checks if all the required packages are installed
# If all packages present, check if they are in the same location (except openssl)
# If all in same location, set the environment variable C_ARCH_LIB_PATH and C_ARCH_BIN_PATH

declare -a libs=("libglog.a" "libgflags.a" "libgmock.a" "libgtest.a" "libprotoc.a" "libprotobuf.a" "libssl.a" "libcrypto.a" "protoc")
libs_len=${#libs[@]}
FOUND=1
for (( i=0; i<${libs_len}; i++ ));
do
	lib_loc[$i]=$(find /usr -name ${libs[$i]} -type f -exec dirname {} \;)
	if [[ -z ${lib_loc[$i]} ]]
	then
		echo "${libs[$i]} is not present. "
		FOUND=0
	fi
done

if [ $FOUND == 0 ]
then
	echo "Please install them"
	return 1
fi

FILE=/usr/include/security/pam_appl.h
if [ ! -f "$FILE" ]; then
    echo "$FILE does not exist. Please install using 'sudo apt-get install libpam0g-dev'"
    return 1
fi

RESULT=0
for x in ${lib_loc[0]}
do
	FOUND=0
	for (( i=1; i<${libs_len}-3; i++ ));
	do
		[[ ${lib_loc[$i]} =~ (^|[[:space:]])$x($|[[:space:]]) ]] && FOUND=1 || FOUND=0
    	if [ $FOUND == 0 ]
	    then
	    	break
	    fi
	done
	if [ $FOUND == 1 ]
	then
		RESULT=1
		break
	fi
done

if [ $RESULT == 1 ]
then
	export C_ARCH_LIB_PATH=${x%/*}
	export C_ARCH_BIN_PATH=${x%/*}
	echo "C_ARCH_LIB_PATH and C_ARCH_BIN_PATH is set to - ${x%/*}"
	echo "Good to compile on x86"
else
	echo "Please ensure you have the libs in the same location"
	return 1
fi
