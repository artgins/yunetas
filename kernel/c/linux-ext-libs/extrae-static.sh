#!/bin/bash

source ./repos2clone-static.sh

if ! command -v ldconfig >/dev/null 2>&1; then
    echo "ldconfig is not available in PATH. Exiting. Add /usr/sbin/ to PATH"
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "ninja is not available in PATH. Exiting. Run 'sudo apt install ninja-build'"
    exit 1
fi


#  Exit immediately if a command exits with a non-zero status.
set -e

#----------------------------------------#
#       Remove build
#----------------------------------------#
rm -rf build/
mkdir build
cd build


echo ""
echo "=======================CLONING====================="

# Loop through the list of repositories and clone each one
for REPO_URL in "${!REPOS[@]}"; do
    VERSION="${REPOS[$REPO_URL]}"

    # Extract the repository name from the URL
    REPO_NAME=$(basename -s .git "$REPO_URL")

    # Clone the repository with the specified version
    echo ""
    echo "===================> Cloning $REPO_NAME from $REPO_URL"
    git clone --recurse-submodules "$REPO_URL"

    # Optional: Print a message after each repository is cloned
    echo "<=================== Finished cloning $REPO_NAME"
    echo ""
done

# Optional: Print a final message
echo "All repositories have been cloned."

cd ..
