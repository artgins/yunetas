# Installation


## System Requirements

- python 3.7+

## Install system dependencies

Use `apt` to install the required dependencies::

    sudo apt install --no-install-recommends \
      git mercurial make cmake ninja-build \
      gcc musl musl-dev musl-tools clang \
      python3-dev python3-pip python3-setuptools python3-tk python3-wheel python3-venv \
      libjansson-dev libpcre2-dev perl dos2unix

## Install [pipx]

[pipx] is used to install Python CLI applications globally while still isolating them in virtual environments.

On Linux:

- Ubuntu 23.04 or above:

    ``` shell
    sudo apt update
    sudo apt install pipx
    pipx ensurepath
    ```

- Ubuntu 22.04 or below

    ``` shell
    python3 -m pip install --user pipx
    python3 -m pipx ensurepath
    ```

## Install CLI :ref:`yunetas`

-
  ``` shell
  pipx install yunetas
  ```

How Update or Uninstall TUI [yunetas]? click below:

```{toggle}

:::

- Update yunetas:

    ``` shell
    pipx upgrade yunetas
    ```

- Uninstall yunetas:

    ``` shell
    pipx uninstall yunetas
    ```

:::

```

::::{tab-set}

:::{tab-item} Label1
Content 1
:::

:::{tab-item} Label2
Content 2
:::

::::


Here is a reference to [My Section][my-section].

[my-section]: #my-section "Go to My Section"

## My Section
This is the content of My Section.




## Clone

Clone with submodules::

    cd ~/yunetaproject

    git clone --recurse-submodules https://github.com/artgins/yunetas.git

    cd ~/yunetaproject/yunetas
    source yunetas-env.sh


    git clone -b 7.0.0a0 --recurse-submodules https://github.com/artgins/yunetas.git yunetas-7.0.0a0

## Install additional Python dependencies

Install additional Python dependencies::

    pip install -r ~/yunetaproject/yunetas/scripts/requirements.txt

## Configure .bashrc

Next times to activate yunetas environment,
(you can add these lines to ``~/.bashrc`` ::

    source ~/yunetaproject/.yuneta/bin/activate
    cd ~/yunetaproject/yunetas
    source yunetas-env.sh

## Configuring (Kconfig)

Configuration options are defined in ``Kconfig`` file.
The output from Kconfig is a header file ``yuneta_config.h`` with macros that can be tested at build time.

You can use any of this utilities to edit the Kconfig file:

     - kconfig-conf Kconfig
     - kconfig-mconf Kconfig
     - kconfig-nconf Kconfig
     - kconfig-qconf Kconfig

## Compiling and Installing

To build and install, with debug and tests::

    mkdir build && cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
    ninja
    ninja install
    ctest    # to run tests


To build without debug::

    mkdir build && cd build
    cmake -GNinja ..
    ninja
    ninja install
    ctest    # to run tests

By default, the installation directory of include files,
libraries and binaries is ``/yuneta/development/outputs/``

[pipx]:     https://pipx.pypa.io/stable/installation/
[yunetas]:  https://pypi.org/project/yunetas/

