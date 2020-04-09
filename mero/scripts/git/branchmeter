#!/usr/bin/env bash
#
# Print information about the branch
#
usage()
{
	echo "Usage: $(basename $0) [options]"
	echo "Options:"
	echo "    -h|--help       -- print this help message"
	echo "    -c|--current    -- show information for current branch only"
}

print_branch_skew()
{
	#
	# Get the last component in the path
	#
	name=`basename $1`

	#
	# Determine how ahead is master
	#
	commit_lag=`git rev-list ${1}..origin/master | wc -l`

	#
	# Check last merge date to/from master
	#
	last_merge=`git log -1 --merges --grep='Merge.*master' --pretty=short --format="%H %ai" origin/master..${1} |  awk '{print $2}'`

	comment=''

	if [ "$last_merge" == "" ]; then
		#
		# If there are no merges, check the date of the first commit in branch which is not included in master
		#
		last_merge=`git log --pretty=short --format="%H %ai" origin/master..${1} | tail -1 | awk '{ print $2}'`

		if [ "$last_merge" == "" ]; then
			#
			# It means that branch doesn't have any specific commits and fully merged into master
			#
			last_merge=`git log -1 --pretty=short --format="%H %ai" ${1} | awk '{ print $2}'`
			comment="(in master)"
		fi
	fi

	#
	# Print branch information.
	#
	printf "%-30s\t%6d\t\t%s %s\n" $name $commit_lag $last_merge "$comment"
}


###########
# Main
###########

current_branch_only=false

case $1 in
	-h|--help) usage; exit 1 ;;
	-c|--current) current_branch_only=true ;;
esac

#
# Check if the CWD is a work dir for a git repository.
#
if ! git rev-parse --git-dir &> /dev/null ; then
	echo "This is not a working dir for git repository"
	exit 1
fi

#
# Get list of branches
#
if $current_branch_only; then
	branch_list=`git name-rev --name-only HEAD`
else
	branch_list=`git branch -r | grep -v master`
fi
printf "Branch Name \t Commits behind master \t    Last merge (or branch) date\n"

#
# Print required information for each branch
#
for branch in $branch_list; do
	print_branch_skew $branch
done
