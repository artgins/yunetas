#
#   Set Yunetas environment
#

if [[ "$0" == -* ]]; then
    name="conda"
else
    name=$(basename "$0")
fi

if [ "$name" == "yunetas-env.sh" ]; then
    echo "Source this file (do NOT execute it!) to set the Yunetas Kernel environment."
    echo "Usage: source yunetas-env.sh"
    exit
fi

if [ ! -f "./YUNETA_VERSION" ]; then
    echo "Source this file must be executed inside yunetas directory."
    exit
fi

# Save the original prompt and PATH (only if not already saved)
if [[ -z "${ORIGINAL_PS1-}" ]]; then
    ORIGINAL_PS1="${PS1-}"
    export ORIGINAL_PS1
fi

if [[ -z "${ORIGINAL_PATH-}" ]]; then
    ORIGINAL_PATH="${PATH}"
    export ORIGINAL_PATH
fi

# Identify OS source tree root directory
builtin cd "$( dirname "$dir" )" > /dev/null
YUNETAS_BASE=$(pwd ${pwd_opt})
export YUNETAS_BASE
unset pwd_opt

# Calculate parent directory of YUNETAS_BASE (local variable)
PARENT_YUNETAS_BASE=$(dirname "${YUNETAS_BASE}")

# Set YUNETAS_OUTPUTS variable
YUNETAS_OUTPUTS="${PARENT_YUNETAS_BASE}/outputs"
export YUNETAS_OUTPUTS

# Set YUNETAS_YUNOS variable
YUNETAS_YUNOS="${PARENT_YUNETAS_BASE}/outputs/yunos"
export YUNETAS_YUNOS

# Add /yuneta/bin to PATH with top priority
yuneta_bin_path="/yuneta/bin"
if ! echo "${PATH}" | grep -q "^${yuneta_bin_path}"; then
    PATH=${yuneta_bin_path}:${PATH}
    export PATH
    echo "Added '/yuneta/bin' to PATH with top priority."
fi

# Add scripts to PATH
scripts_path=${YUNETAS_BASE}/scripts
if ! echo "${PATH}" | grep -q "${scripts_path}"; then
    PATH=${scripts_path}:${PATH}
    export PATH
fi
unset scripts_path

# Enable custom environment settings
yunetas_answer_file=~/.yunetasrc
if [ -f "${yunetas_answer_file}" ]; then
    . "${yunetas_answer_file}"
fi
unset yunetas_answer_file

# Change the shell prompt to include "(yunetas)"
if [[ -n "${PS1-}" ]]; then
    PS1="(yunetas) ${PS1}"
    export PS1
fi

# Inform the user
echo "Yunetas environment activated."
echo "YUNETAS_BASE: ${YUNETAS_BASE}"
echo "YUNETAS_OUTPUTS: ${YUNETAS_OUTPUTS}"
echo "YUNETAS_YUNOS: ${YUNETAS_YUNOS}"
echo "To deactivate, run: deactivate_yunetas"

# Function to deactivate the environment
deactivate_yunetas() {
    echo "Deactivating Yunetas environment..."

    # Restore the original prompt and PATH
    if [[ -n "${ORIGINAL_PS1-}" ]]; then
        PS1="${ORIGINAL_PS1}"
        export PS1
        unset ORIGINAL_PS1
    fi

    if [[ -n "${ORIGINAL_PATH-}" ]]; then
        PATH="${ORIGINAL_PATH}"
        export PATH
        unset ORIGINAL_PATH
    fi

    # Unset Yunetas-specific environment variables
    unset YUNETAS_BASE
    unset YUNETAS_YUNOS
    echo "Yunetas environment deactivated."
}
