README
======

This: Directory of documentations.


How to build documentation
==========================

The documentations of this directory is building with [sphinx]
and the nice [sphinx-book-theme].

It has been tested in Kubuntu 23.04.

- Skills required on:
    ``` json
    [
        "linux", "python", 
    ]
    ```

For any python package use, it's highly recommended to use a dedicated virtual environment,
for example [venv] or [conda]. 
Next some short instructions to install one or more of mentioned tools.

Some fixed values are used, modify the scripts as you want. 

- The constant values used for [venv]:
    ``` json
    {
        "venv_name": "venv_sphinx"  #  virtual env name for [venv] 
    }
    ```

- The constant values used for [conda]:
    ``` json
    {
        "venv_name": "conda_sphinx"  #  virtual env name for [conda] 
    }
    ```

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
    exit # exit and RE-open bash to continue !!
    ```
- Create the virtual environment `conda_sphinx` and activate:
    ``` shell
    conda create -n conda_sphinx
    conda activate conda_sphinx
    conda install pip
    ```
Install [sphinx] and [sphinx-book-theme]  
----------------------------------------

Old and new generations.

- In any virtual environment, install [sphinx], [sphinx-book-theme] and some utils extensions:
  - [ablog](https://ablog.readthedocs.io/en/stable/)
  - [myst_nb](https://myst-nb.readthedocs.io/en/latest/)
  - [numpydoc](https://numpydoc.readthedocs.io/en/latest/format.html) 
  - [matplotlib](https://matplotlib.org/)
  - [sphinx_design](https://sphinx-design.readthedocs.io/en/latest/)
  - [sphinx_copybutton](https://sphinx-copybutton.readthedocs.io/en/latest/)
  - [sphinx_tabs](https://sphinx-tabs.readthedocs.io/en/latest/)
  - [sphinx_togglebutton](https://sphinx-togglebutton.readthedocs.io/en/latest/)
  - [sphinxext.opengraph](https://sphinxext-opengraph.readthedocs.io/en/latest/)
  - [sphinxcontrib.youtube](https://sphinxcontrib-youtube.readthedocs.io/en/latest/)
  - [sphinx_thebe](https://sphinx-thebe.readthedocs.io/en/latest/)
  - [sphinxcontrib.bibtex](https://sphinxcontrib-bibtex.readthedocs.io/en/latest/)
  - [sphinxcontrib.mermaid](https://sphinxcontrib-mermaid-demo.readthedocs.io/en/latest/)

-
    ```
    pip install sphinx sphinx-book-theme myst-parser \
        ablog myst_nb numpydoc matplotlib \
        sphinx_design \
        sphinx_copybutton \
        sphinx_tabs \
        sphinx_togglebutton \
        sphinxext.opengraph \
        sphinxcontrib.youtube \
        sphinx_thebe \
        sphinxcontrib.bibtex \
        sphinxcontrib.mermaid
    ```

Compile the documentation
=========================

- To generate the documentation:

    ``` shell
    cd doc.yuneta.io
    make html
    ```
- The output, i.e., the static web application, 
    will be installed in `_build` directory. 
    From here deploy where you want.


[sphinx]:   https://www.sphinx-doc.org/
[venv]:     https://docs.python.org/3/library/venv.html
[conda]:    https://docs.anaconda.com/free/miniconda/#miniconda
[sphinx-book-theme]: https://sphinx-book-theme.readthedocs.io/en/stable/