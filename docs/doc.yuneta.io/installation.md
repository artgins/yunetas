# **Installation**

## License

This project is licensed under the MIT License, except for the following files:

- kernel/c/libjwt/*
  Mozilla Public License 2.0 (MPL-2.0)


## System Requirements

- [python](https://www.python.org/) 3.7+


**Dependencies** for C and Linux:
- [Jansson](http://jansson.readthedocs.io/en/latest/) MIT License
- [libjwt](https://github.com/benmcollins/libjwt) MPL-2.0 License
- [liburing](https://github.com/axboe/liburing) MIT License, LGPL-2.1, GPL-2.0  
- [mbedtls](https://www.trustedfirmware.org/projects/mbed-tls/) Apache-2.0 or GPL-2.0
- [openssl](https://www.openssl.org/) Apache-2.0 License
- [pcre2](https://github.com/PCRE2Project/pcre2) BSD License and others
- [libbacktrace](https://github.com/ianlancetaylor/libbacktrace) BSD 3-Clause License
- [argp-standalone](https://github.com/artgins/argp-standalone.git) LGPL-2.1 License
- [ncurses](https://github.com/mirror/ncurses.git) MIT License


**Dependencies** for deploying in Linux: 
- [nginx](https://nginx.org) BSD-2-Clause license
- [openresty](https://openresty.org/) BSD 2-Clause, BSD 3-Clause, MIT, OpenSSL, Zlib, SSLeay
- [curl](https://github.com/curl/curl) curl license, MIT/X derivate license,

## Create environment

Firstly you must create the user/group `yuneta` and the directory `/yuneta`.

    sudo adduser yuneta
    sudo mkdir /yuneta
    sudo chown yuneta:yuneta /yuneta

Re-enter with the user yuneta

## Install system dependencies

Install the C dependencies:

    sudo apt -y install --no-install-recommends \
      git mercurial make cmake ninja-build \
      gcc musl musl-dev musl-tools clang g++ \
      python3-dev python3-pip python3-setuptools \
      python3-tk python3-wheel python3-venv \
      libjansson-dev libpcre2-dev liburing-dev libcurl4-openssl-dev \
      libpcre3-dev zlib1g-dev libssl-dev \
      perl dos2unix tree curl \
      postgresql-server-dev-all libpq-dev \
      kconfig-frontends telnet pipx \
      patch gettext

    pipx install kconfiglib

<details>
<summary>Why these dependencies?</summary>
<pre>
  libjansson-dev          # required for libjwt
  libpcre2-dev            # required by openresty
  perl dos2unix mercurial # required by openresty
  pipx kconfiglib         # used by yunetas, configuration tool
  kconfig-frontends       # used by yunetas, other configuration tool
  telnet                  # required by tests
</pre>
</details>

## Install `yunetas`

::::{tab-set}

:::{tab-item} With `pipx` 

## Install `pipx`

[pipx] is used to install Python CLI applications globally while still isolating them in virtual environments.

On Linux:

- Ubuntu 23.04 or above:

    ``` shell
    sudo apt install pipx
    pipx ensurepath
    ```

- Ubuntu 22.04 or below

    ``` shell
    python3 -m pip install --user pipx
    python3 -m pipx ensurepath
    ```

## Install `yunetas`

-
  ``` shell
  pipx install yunetas
  ```

### Update or uninstall `yunetas`

```{dropdown} Click to see
  - Update yunetas:

      ``` shell
      pipx upgrade yunetas
      ```

  - Uninstall yunetas:

      ``` shell
      pipx uninstall yunetas
      ```
```

:::

:::{tab-item} With `conda`


Steps to install and create a virtual environment:
- Install [conda]:

    ``` shell
    mkdir -p ~/miniconda3
    wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
    bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
    rm -rf ~/miniconda3/miniconda.sh
    ~/miniconda3/bin/conda init bash
    ```

- **Now you must close and re-open your current shell**:
    ``` shell
    exit # exit and RE-open bash to continue !!
    ```
- Add conda-forge channel:
    ``` shell
    conda config --add channels conda-forge
    ```

- Create the virtual environment `conda_yunetas` and activate:
    ``` shell
    conda create -y -n conda_yunetas pip
    conda config --set auto_activate_base false
    echo 'conda activate conda_yunetas' >> ~/.bashrc
    echo '' >> ~/.bashrc
    source ~/.bashrc
    ```

## Install `yunetas`

``` shell
pip install yunetas
```


:::

::::

## Clone

Clone Yunetas with submodules:

Build your own project directory:

    mkdir ~/yunetaprojects 
    cd ~/yunetaprojects

Get the current version of yunetas:

    git clone --recurse-submodules https://github.com/artgins/yunetas.git

Or Get some version of yunetas:

    git clone -b <version> --recurse-submodules https://github.com/artgins/yunetas.git <version>

## Activating yunetas

Go to the yunetas directory in your project and activate:

    cd ~/yunetaprojects/yunetas
    source yunetas-env.sh


## Configure .bashrc

Next times, to activate yunetas environment,
(you can add these lines to ``~/.bashrc`` :

    # edit: "vim ~/.bashrc" and add next lines: 
    cd ~/yunetaprojects/yunetas
    source yunetas-env.sh

## Configure .yunetasrc

The script source `yunetas-env.sh` also sources the file 

    ~/.yunetasrc

where you can place your own scripts.

### Configuring (Kconfig)

Configuration options are defined in ``Kconfig`` file.
The output from Kconfig is a header file ``yuneta_config.h`` with macros that can be tested at build time.

Goto `yunetas` directory:

    cd ~/yunetaprojects/yunetas

Use this utility to edit the Kconfig file and to select the compiler, build type, etc:

    menuconfig

> ⚠️ **Warning:** Save the configuration, otherwise the compilation will fail, the .config file is required.


## Install dependencies

Firstly, install yuneta dependencies:

Goto `linux-ext-libs` directory:

    cd ~/yunetaprojects/yunetas/kernel/c/linux-ext-libs/

Extract, compile and install:

    # Version non-static (closely static)
    ./extrae.sh
    ./configure-libs.sh

    # Version static (ONLY if you want to use MUSL compiler)
    ./extrae-static.sh
    ./configure-libs-static.sh

## Compile Yunetas

### Compiling and Installing Yunetas

To build and install yunetas:

    yunetas init
    yunetas build


### Test

To test:

    yunetas test

By default, the installation directory of include files,
libraries and binaries will be in ``/yuneta/development/outputs/``

[pipx]:     https://pipx.pypa.io/stable/installation/
[yunetas]:  https://pypi.org/project/yunetas/

[sphinx]:   https://www.sphinx-doc.org/
[venv]:     https://docs.python.org/3/library/venv.html
[conda]:    https://docs.anaconda.com/free/miniconda/#miniconda
[sphinx-book-theme]: https://sphinx-book-theme.readthedocs.io/en/stable/
