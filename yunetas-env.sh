#
#   Set Yunetas environment
#

name=$(basename "$0")
if [ "X$name" "==" "Xyunetas-env.sh" ]; then
    echo "Source this file (do NOT execute it!) to set the Yunetas Kernel environment."
    exit
fi

# You can further customize your environment by creating a bash script called
# .yunetasrc in your home directory. It will be automatically
# run (if it exists) by this script.

# identify OS source tree root directory
export YUNETAS_BASE=$( builtin cd "$( dirname "$dir" )" > /dev/null && pwd ${pwd_opt})
unset pwd_opt

scripts_path=${YUNETAS_BASE}/scripts
if ! echo "${PATH}" | grep -q "${scripts_path}"; then
    export PATH=${scripts_path}:${PATH}
fi
unset scripts_path

# enable custom environment settings
yunetas_answer_file=~/.yunetasrc
[ -f ${yunetas_answer_file} ] &&  . ${yunetas_answer_file}
unset yunetas_answer_file
