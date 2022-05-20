#!/bin/bash
#########################
# Usage:   ./cp_kinetic_libs.sh -a <Architecture> -d <Kinetic folder>
# Example: ./cp_kineti_libs.sh -a X86 -d /home/user/kineticd
# Description:
#   Copy all libraries and header files from Kinetic that are required to build the
#   Albany Minio project.
#   If no folder is specified, the script looks for a folder with "kinetic" in its 
#   name in the parent directory. 
#########################
albany_path=`pwd`
kineticd_path=""
architecture="x86"

while getopts a:d: flag
do
    case "${flag}" in
        a) architecture=${OPTARG};;
        d) kineticd_path=${OPTARG};;
    esac
done

# Find the directory containing kineticd
cd ..
if [ "$kineticd_path" = "" ]
then
   ls_output=`ls | grep kineticd`
   if [ "$ls_output" = "" ];
   then
      echo "Error: Kineticd directory can not be found"
      exit 1
   fi
   kineticd_path=`echo $ls_output | cut -d " " -f1`
   n_folders=`echo $ls_output | wc -w`
   if [ $n_folders -gt 1 ]
   then
      echo "Warning: More than one folder containing \"kineticd\" was found. Copying the libraries from \"$kineticd_path\""
   fi
fi
echo "Copying the kinetic libraries from \"$kineticd_path\""
cd $kineticd_path

# Copy the libraries over
if [ "$architecture" = "x86" ]
then
   ./cplibx86.sh $albany_path/lib
else
   if [ "$architecture" = "arm" ]
   then
      ./cplibarm.sh $albany_path/lib
   else
      echo "Error: architecture \"$architecture\" unknown"
      exit 1
   fi
fi

exit $? # Return error code for the script that copied the libraries
