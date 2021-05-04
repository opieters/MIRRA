#!/bin/bash

# Call this file with `bash ./project-create.sh project-name [service-name]`
# - project-name is mandatory
# - service-name is optional

# This will creates 4 directories and a git `post-receive` hook.
# The 4 directories are:
# - $GIT: a git repo
# - $TMP: a temporary directory for deployment
# - $WWW: a directory for the actual production files
# - $ENV: a directory for the env variables

# When you push your code to the git repo,
# the `post-receive` hook will deploy the code
# in the $TMP directory, then copy it to $WWW.

DIR_TMP="/srv/tmp/"
DIR_WWW="/srv/www/"
DIR_GIT="/srv/git/"
DIR_ENV="/srv/env/"

# add your user to the users group
sudo adduser $USER users

function dir_create() {
	sudo mkdir -p "$1"
	cd "$1" || exit

	# Define group recursively to "users", on the directories
	sudo chgrp -R users .

	# Define permissions recursively, on the sub-directories
	# g = group, + add rights, r = read, w = write, X = directories only
	# . = curent directory as a reference
	sudo chmod -R g+rwX .
}


if [ $# -eq 0 ]; then
	echo 'No project name provided (mandatory)'
	exit 1
else
	echo "- Project name:" "$1"
fi

if [ -z "$2" ]; then
	echo '- Service name (optional): not provided'
	GIT=$DIR_GIT$1.git
	TMP=$DIR_TMP$1
	WWW=$DIR_WWW$1
	ENV=$DIR_ENV$1
else
	echo "- Service name (optional):" "$2"
	GIT=$DIR_GIT$1.$2.git
	TMP=$DIR_TMP$1.$2
	WWW=$DIR_WWW$1/$2
	ENV=$DIR_ENV$1/$2

	# Create $WWW parent directory
	dir_create "$DIR_WWW$1"
fi

dir_create $DIR_TMP
dir_create $DIR_WWW

echo "- git:" "$GIT"
echo "- tmp:" "$TMP"
echo "- www:" "$WWW"
echo "- env:" "$ENV"

export GIT
export TMP
export WWW
export ENV

# Create a directory for the env repository
sudo mkdir -p "$ENV"
cd "$ENV" || exit
sudo touch .env

# Create a directory for the git repository
sudo mkdir -p "$GIT"
cd "$GIT" || exit

# Init the repo as an empty git repository
sudo git init --bare

# Define group recursively to "users", on the directories
sudo chgrp -R users .

# Define permissions recursively, on the sub-directories
# g = group, + add rights, r = read, w = write, X = directories only
# . = curent directory as a reference
sudo chmod -R g+rwX .
# Sets the setgid bit on all the directories
# https://www.gnu.org/software/coreutils/manual/html_node/Directory-Setuid-and-Setgid.html
sudo find . -type d -exec chmod g+s '{}' +

# Make the directory a shared repo
sudo git config core.sharedRepository group

# Creating the git hook file
cd hooks || exit

# create a post-receive file
sudo tee post-receive <<EOF
#!/bin/bash

# The production directory
WWW="${WWW}"

# A temporary directory for deployment
TMP="${TMP}"

# The Git repo
GIT="${GIT}"

# The Env repo
ENV="${ENV}"

# Deploy the content to the temporary directory
mkdir -p \$TMP
git --work-tree=\$TMP --git-dir=\$GIT checkout -f

# Copy the env variable to the temporary directory
cp -a \$ENV/. \$TMP

# Do stuffs, like npm install
cd \$TMP || exit


# TODO: remove the
#    * database
#    * web-application container
# And use the docker compose to put the other containers up

# Replace the content of the production directory
# with the temporary directory
cd / || exit
rm -rf \$WWW
mv \$TMP \$WWW

# Do stuff like starting docker
cd \$WWW || exit
# docker-compose up -d --build
EOF

# make it executable
sudo chmod +x post-receive
