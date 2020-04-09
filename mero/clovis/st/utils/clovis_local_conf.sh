#!/bin/bash

# Get local address and other parameters to start services
modprobe lnet &>> /dev/null
lctl network up &>> /dev/null
LOCAL_NID=`lctl list_nids | head -1`
if [ X$LOCAL_NID == X ]; then
	echo "lnet is not up"
	exit
fi

if [ X$CLOVIS_LOCAL_EP == X ]; then
	CLOVIS_LOCAL_EP=$LOCAL_NID:12345:34:101
	CLOVIS_LOCAL_EP2=$LOCAL_NID:12345:34:102
fi

if [ X$CLOVIS_HA_EP == X ]; then
	CLOVIS_HA_EP=$LOCAL_NID:12345:34:1
fi

if [ X$CLOVIS_PROF_OPT == X ]; then
	CLOVIS_PROF_OPT=0x7000000000000001:0
fi

if [ X$CLOVIS_PROC_FID == X ]; then
	CLOVIS_PROC_FID=0x7200000000000000:0
fi
