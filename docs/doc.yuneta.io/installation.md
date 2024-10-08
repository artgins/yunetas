# Installation


## System Requirements

- python 3.7+

## Install system dependencies

Use `apt` to install the required dependencies

    sudo apt install --no-install-recommends \
      git mercurial make cmake ninja-build \
      gcc musl musl-dev musl-tools clang \
      python3-dev python3-pip python3-setuptools python3-tk python3-wheel python3-venv \
      libjansson-dev libpcre2-dev perl dos2unix liburing-dev

## Install `yunetas`

::::{tab-set}

:::{tab-item} With `pipx` 

## Install `pipx`

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
:::

::::



Here is a reference to  {ref}`python-packages`

(python-packages)=
## Python Packages

- Install python packages:

    ``` shell
    pip install cement
    pip install plumbum
    pip install fastapi
    pip install "uvicorn[standard]"
    pip install "typer[all]"
    ```

## Clone

Clone Yunetas with submodules:

Build your own project directory:

    mkdir ~/yunetaprojects 
    cd ~/yunetaprojects

Get the current version of yunetas:

    git clone --recurse-submodules https://github.com/artgins/yunetas.git

Or Get some version of yunetas:

    git clone -b 7.0.0a0 --recurse-submodules https://github.com/artgins/yunetas.git yunetas-7.0.0a0

## Activating yunetas

Go to the yunetas directory in your project and activate:

    cd ~/yunetaprojects/yunetas
    source yunetas-env.sh


## Install additional Python dependencies

Install additional Python dependencies (`scripts` directory will be added to path):

    # this don't work
    cd ./scripts
    pip install -r requirements.txt

    # use this
    pipx install kconfiglib

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

## Configuring (Kconfig)

Configuration options are defined in ``Kconfig`` file.
The output from Kconfig is a header file ``yuneta_config.h`` with macros that can be tested at build time.

Use this utility to edit the Kconfig file:

    cd ~/yunetaprojects/yunetas
    menuconfig

> ⚠️ **Warning:** Save the configuration, otherwise the compilation will fail, the .config file is required.

## Compiling and Installing

Firstly, install dependencies:

    cd ~/yunetaprojects/yunetas/kernel/c/root-linux-ext-libs/
    ./extrae.sh
    ./configure-libs.sh



To build and install, with debug and tests:

    cd ~/yunetaprojects/yunetas
    mkdir build 
    cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
    ninja
    ninja install
    ctest    # to run tests


To build without debug:

    mkdir build && cd build
    cmake -GNinja ..
    ninja
    ninja install
    ctest    # to run tests

By default, the installation directory of include files,
libraries and binaries is ``/yuneta/development/outputs/``

[pipx]:     https://pipx.pypa.io/stable/installation/
[yunetas]:  https://pypi.org/project/yunetas/

[sphinx]:   https://www.sphinx-doc.org/
[venv]:     https://docs.python.org/3/library/venv.html
[conda]:    https://docs.anaconda.com/free/miniconda/#miniconda
[sphinx-book-theme]: https://sphinx-book-theme.readthedocs.io/en/stable/
