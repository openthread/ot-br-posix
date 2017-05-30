#! /bin/sh

# Copyright (c) 2016 Nest Labs, Inc.
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


display_usage ()
{
	echo "Thread Network Retain - Save/Recall thread network info"
	echo ""
	echo "Usage: $(basename $0) [primary-net-info-file] [secondary-net-info-file] [wpanctl path] [command]"
	echo ""
	echo "    Command:"
	echo "        'S' or 'save'   : Save current network info."
	echo "        'R' or 'recall' : Recall/Restore previously saved network info."
	echo "        'E' or 'erase'  : Erase previously saved network info."
	echo ""
	echo "     [primary-net-info-file] and [secondary-net-info-file] give full path to files to store network info."
	echo "     [wpanctl path] gives the full path to 'wpanctl'."
	echo ""
}

# Check the input args

if [ $# -ne 4 ]; then
	display_usage
	exit 1
fi

primary_file_name="$1"
secondary_file_name="$2"
wpanctl_command="$3"
retain_command=$4

# List of all wpantund properties saved
WPANTUND_PROP_NAME="Network:Name"
WPANTUND_PROP_KEY="Network:Key"
WPANTUND_PROP_PANID="Network:PANID"
WPANTUND_PROP_XPANID="Network:XPANID"
WPANTUND_PROP_CHANNEL="NCP:Channel"
WPANTUND_PROP_MACADDR="NCP:MACAddress"
WPANTUND_PROP_NODE_TYPE="Network:NodeType"
WPANTUND_PROP_KEY_INDEX="Network:KeyIndex"
WPANTUND_PROP_ROUTER_ID="Thread:RouterID"
WPANTUND_PROP_PREFERRED_ROUTER_ID="Thread:PreferredRouterID"

MAX_ROUTER_ID=60

WPANTUND_PROP_IS_COMMISSIONED="Network:IsCommissioned"
WPANTUND_PROP_AUTO_RESUME="Daemon:AutoAssociateAfterReset"

cur_name=
cur_key=
cur_panid=
cur_xpanid=
cur_channel=
cur_macaddr=
cur_type=
cur_keyindex=
cur_routerid=

new_name=
new_key=
new_panid=
new_xpanid=
new_channel=
new_macaddr=
new_type=
new_keyindex=
new_routerid=

# Populate the new network info variables by reading the values from wpanctl
get_new_network_info_from_wpantund ()
{
	val=$($wpanctl_command get -v $WPANTUND_PROP_NAME)
	val=${val#\"}
	new_name=${val%\"}

	val=$($wpanctl_command get $WPANTUND_PROP_KEY)
	val=${val#*\[}
	new_key=${val%\]*}

	val=$($wpanctl_command get $WPANTUND_PROP_PANID)
	new_panid=$(echo "$val" | sed -e 's/.*=[[:space:]]*//')

	val=$($wpanctl_command get $WPANTUND_PROP_XPANID)
	new_xpanid=$(echo "$val" | sed -e 's/.*=[[:space:]]*0x//')

	val=$($wpanctl_command get $WPANTUND_PROP_CHANNEL)
	new_channel=$(echo "$val" | sed -e 's/.*=[[:space:]]*//')

	val=$($wpanctl_command get $WPANTUND_PROP_MACADDR)
	val=${val#*\[}
	new_macaddr=${val%\]*}

	val=$($wpanctl_command get $WPANTUND_PROP_NODE_TYPE)
	new_type=$(echo "$val" | sed -e 's/.*=[[:space:]]*//')

	val=$($wpanctl_command get $WPANTUND_PROP_KEY_INDEX)
	new_keyindex=$(echo "$val" | sed -e 's/.*=[[:space:]]*//')

	val=$($wpanctl_command get -v $WPANTUND_PROP_ROUTER_ID)
	new_routerid=$(printf "%d" $val)
}

restore_network_info_on_wpantund ()
{
	# Calculate a new router id value by adding one to it
	# This ensures router id is different upon every reset/restore
	router_id=$(($cur_routerid+1))

	if [ "$router_id" -ge "$MAX_ROUTER_ID" ]
	then
		router_id=0
	fi
	$wpanctl_command set $WPANTUND_PROP_PREFERRED_ROUTER_ID $router_id

	$wpanctl_command set $WPANTUND_PROP_MACADDR $cur_macaddr
	$wpanctl_command set $WPANTUND_PROP_NAME $cur_name
	$wpanctl_command set $WPANTUND_PROP_KEY -d $cur_key
	$wpanctl_command set $WPANTUND_PROP_PANID $cur_panid
	$wpanctl_command set $WPANTUND_PROP_XPANID -d $cur_xpanid
	$wpanctl_command set $WPANTUND_PROP_CHANNEL $cur_channel
	$wpanctl_command set $WPANTUND_PROP_NODE_TYPE $cur_type
	$wpanctl_command set $WPANTUND_PROP_KEY_INDEX $cur_keyindex

	$wpanctl_command attach
}

is_ncp_commissioned ()
{
	val=$($wpanctl_command get -v $WPANTUND_PROP_IS_COMMISSIONED)

	if [ "$val" != "true" ]; then
		return 1
	fi

	return 0
}

is_auto_resume_enabled ()
{
	val=$($wpanctl_command get -v $WPANTUND_PROP_AUTO_RESUME)

	if [ "$val" != "true" ]; then
		return 1
	fi

	return 0
}

verify_cur_info ()
{
	if [ -z "$cur_name" ] ||		\
	   [ -z "$cur_key" ] || 		\
	   [ -z "$cur_panid" ] || 		\
	   [ -z "$cur_xpanid" ] ||		\
	   [ -z "$cur_channel" ] || 	\
	   [ -z "$cur_macaddr" ] || 	\
	   [ -z "$cur_type" ] ||		\
	   [ -z "$cur_keyindex" ] || 	\
	   [ -z "$cur_routerid" ]
	then
		return 1
	fi

	return 0
}

verify_new_info ()
{

	if [ -z "$new_name" ] ||		\
	   [ -z "$new_key" ] || 		\
	   [ -z "$new_panid" ] || 		\
	   [ -z "$new_xpanid" ] ||		\
	   [ -z "$new_channel" ] || 	\
	   [ -z "$new_macaddr" ] || 	\
	   [ -z "$new_type" ] ||		\
	   [ -z "$new_keyindex" ] ||    \
	   [ -z "$new_routerid" ]
	then
		return 1
	fi

	return 0
}

is_new_info_same_as_cur_info ()
{
	if [ "$cur_name" != "$new_name" ] || \
	   [ "$cur_key" != "$new_key" ] || \
	   [ "$cur_panid" != "$new_panid" ] || \
	   [ "$cur_xpanid" != "$new_xpanid" ] || \
	   [ "$cur_channel" != "$new_channel" ] || \
	   [ "$cur_macaddr" != "$new_macaddr" ] || \
	   [ "$cur_type" != "$new_type" ] || \
	   [ "$cur_keyindex" != "$new_keyindex" ] || \
	   [ "$cur_routerid" != "$new_routerid" ]
	then
		return 1
	fi

	return 0
}

# Reads and parses the network info from a file and populate old network info variables
#     First arg ($1) is the file name
read_cur_network_info_from_file ()
{
	if [ -r $1 ]; then

		while IFS="\= " read -r key value; do

			case $key in
			$WPANTUND_PROP_NAME)
			  cur_name=$value
			  ;;
			$WPANTUND_PROP_KEY)
			  cur_key=$value
			  ;;
			$WPANTUND_PROP_PANID)
			  cur_panid=$value
			  ;;
			$WPANTUND_PROP_XPANID)
			  cur_xpanid=$value
			  ;;
			$WPANTUND_PROP_CHANNEL)
			  cur_channel=$value
			  ;;
			$WPANTUND_PROP_MACADDR)
			  cur_macaddr=$value
			  ;;
			$WPANTUND_PROP_NODE_TYPE)
			  cur_type=$value
			  ;;
			$WPANTUND_PROP_KEY_INDEX)
			  cur_keyindex=$value
			  ;;
			$WPANTUND_PROP_ROUTER_ID)
			  cur_routerid=$value
			  ;;
			esac

		done < $1
	fi
}

# Writes the new info into a given file name (first arg should be filename)
write_new_info_to_file ()
{
	fname=$1

	if [ -e $fname ]; then
		rm $fname > /dev/null 2>&1
	fi

	mkdir -p $(dirname "$fname")

	# Write all the contents to the file

	echo "${WPANTUND_PROP_NAME} = $new_name" >> $fname
	echo "${WPANTUND_PROP_KEY} = $new_key" >> $fname
	echo "${WPANTUND_PROP_PANID} = $new_panid" >> $fname
	echo "${WPANTUND_PROP_XPANID} = $new_xpanid" >> $fname
	echo "${WPANTUND_PROP_CHANNEL} = $new_channel" >> $fname
	echo "${WPANTUND_PROP_MACADDR} = $new_macaddr" >> $fname
	echo "${WPANTUND_PROP_NODE_TYPE} = $new_type" >> $fname
	echo "${WPANTUND_PROP_KEY_INDEX} = $new_keyindex" >> $fname
	echo "${WPANTUND_PROP_ROUTER_ID} = $new_routerid" >> $fname

	# Add a time stamp
	echo "# Saved on " $(date) >> $fname
}

# For test and debugging only
display_cur_network_info ()
{
	echo "cur_name is"  $cur_name
	echo "cur_key is"  $cur_key
	echo "cur_panid is" $cur_panid
	echo "cur_xpanid is" $cur_xpanid
	echo "cur_channel is" $cur_channel
	echo "cur_macaddr is" $cur_macaddr
	echo "cur_type is" $cur_type
	echo "cur_keyindex is"  $cur_keyindex
	echo "cur_routerid is"  $cur_routerid
}

# For test and debugging only
display_new_network_info ()
{
	echo "new_name is"  $new_name
	echo "new_key is"  $new_key
	echo "new_panid is" $new_panid
	echo "new_xpanid is" $new_xpanid
	echo "new_channel is" $new_channel
	echo "new_macaddr is" $new_macaddr
	echo "new_type is" $new_type
	echo "new_keyindex is" $new_keyindex
	echo "new_routerid is" $new_routerid
}

save_network_info ()
{
	get_new_network_info_from_wpantund

	read_cur_network_info_from_file $primary_file_name

	if verify_new_info; then

		if is_new_info_same_as_cur_info; then
			echo "Saved network info in \"$primary_file_name\" is up-to-date and valid."
		else

			write_new_info_to_file $secondary_file_name

			# verify the file content match what was written

			read_cur_network_info_from_file $secondary_file_name

			if is_new_info_same_as_cur_info; then

				cp $secondary_file_name $primary_file_name

				echo  "Successfully saved network info in \"$primary_file_name\" and \"$secondary_file_name\""

			else
				echo "ERROR: Could not save the network data"
			fi
			# need to figure out if it fails what to do...
		fi

	else
		echo "ERROR: Network info from wpantund is not valid"
	fi
}

erase_network_info ()
{
	if [ -e $primary_file_name ]; then
		rm $primary_file_name
	fi

	if [ -e $secondary_file_name ]; then
		rm $secondary_file_name
	fi

	echo "Successfully erased network data in \"$primary_file_name\" and \"$secondary_file_name\"".
}

recall_network_info ()
{
	if is_ncp_commissioned; then

		if is_auto_resume_enabled; then
			echo "NCP is commissioned and AutoResume is enabled. Skipping recall."
			return
		else
			echo "NCP is commissioned but AutoResume is not enabled. Skipping recall. Resuming instead."
			$wpanctl_command resume
			return
		fi
	fi

	echo "NCP is NOT commissioned, trying to recall."

	read_cur_network_info_from_file $primary_file_name

	# if no valid data from the main file, try to read from the second (backup) file
	verify_cur_info || read_cur_network_info_from_file $secondary_file_name

	if verify_cur_info; then

		restore_network_info_on_wpantund

		echo "Successfully recalled network info from \"$primary_file_name\""
	else
		echo "No valid saved network info to recall."
	fi
}

case $retain_command in

R|recall)
  recall_network_info
  ;;

S|save)
  save_network_info
  ;;

E|erase)
  erase_network_info
  ;;

*)
  echo "Error: Unknown command \"$retain_command\""
  echo " "
  display_usage
  ;;
esac

exit 0
