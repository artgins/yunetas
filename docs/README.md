How to build documentation
==========================

This documentation is builting with [sphinx].

It has been tested in Kubuntu 23.04.

It's highly recommended to use a dedicated virtual environment,
for example [venv], [conda] or [pipx].

Using [venv]
------------

Steps to install and create a virtual environment:
- Install [venv]
- Create the virtual environment `venv_sphinx`
- Activate the virtual environment

    ``` shell
    sudo apt install python3-venv python3-pip
    python3 -m venv ~/venv_sphinx
    source ~/venv_sphinx/bin/activate
    ```
- Install [sphinx] in the virtual environment:
    ``` bash
    pip install sphinx sphinx-book-theme sphinx_copybutton sphinx_design sphinx_sitemap myst-parser
    ```

Using [conda]
-------------

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
    exit
    ```
- Create the virtual environment `conda_sphinx` and activate:
    ``` shell
    conda create -n conda_sphinx
    conda activate conda_sphinx
    conda install pip
    ```
- Install [sphinx] in the virtual environment:
    ``` bash
    pip install sphinx sphinx-book-theme sphinx_copybutton sphinx_design sphinx_sitemap myst-parser
    ```

Using [pipx]
------------

Pipx is a tool to install and run Python applications in asolated environments.

- Install [pipx]
    ``` shell
    sudo apt install pipx
    pipx ensurepath
    ```
- Install [sphinx] with pipx:
    ``` bash
    pipx install sphinx 
    pipx inject sphinx sphinx-book-theme sphinx_copybutton sphinx_design sphinx_sitemap myst-parser
    ```

Compile the documentation
=========================

To generate the documentation:

``` bash
cd doc.yuneta.io
make html
```


[pipx]:     https://pipx.pypa.io/stable/installation/
[sphinx]:   https://www.sphinx-doc.org/
[venv]:     https://docs.python.org/3/library/venv.html
[conda]:    https://docs.anaconda.com/free/miniconda/#miniconda
