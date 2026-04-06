How to build documentation
==========================

This: Directory of documentations.

The documentation is built with [sphinx],  [sphinx-book-theme] and [MyST Markdown]

It has been tested in Kubuntu 23.04.

- You should be relatively familiar with:
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
        "venv_name": "conda_myst"  #  virtual env name for [conda] 
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
- Add conda-forge channel:
    ``` shell
    conda config --add channels conda-forge
    ```
- Create the virtual environment `conda_myst` and activate:
    ``` shell
    conda create -n conda_myst pip
    conda activate conda_myst
    ```

Install [sphinx] and [sphinx-book-theme]  
----------------------------------------


- In some virtual environment, install [sphinx], [sphinx-book-theme] 
  -
      ``` shell 
    # Instalar (una sola vez)
        npm install -g mystmd
        pip install jupyter matplotlib numpy   # solo para el notebook demo

    # Lanzar el dev server
        cd docs/doc.yuneta.io
        myst start

      ```

### Tested versions

Last known-good versions used to build `doc.yuneta.io` (verified on 2026-04-05,
Python 3.13, conda env `conda_myst`):

| Package | Version |
|---|---|
| Sphinx | 9.1.0 |
| sphinx-book-theme | 1.2.0 |
| myst-parser | 5.0.0 |
| myst-nb | 1.4.0 |
| sphinx-design | 0.7.0 |
| sphinx-copybutton | 0.5.2 |
| sphinx-togglebutton | 0.4.5 |
| sphinx-examples | 0.0.5 |
| sphinx-tabs | 3.5.0 |
| sphinx-thebe | 0.3.1 |
| sphinxext-opengraph | 0.13.0 |
| sphinxcontrib-bibtex | 2.6.5 |
| sphinxcontrib-mermaid | 2.0.1 |
| sphinxcontrib-youtube | 1.5.0 |
| sphinxcontrib-serializinghtml | 2.0.0 |
| ablog | 0.11.13 |
| numpydoc | 1.10.0 |
| matplotlib | 3.10.8 |

Notes:
- `myst-parser` 5.x declares `sphinx>=8,<10`, so it supports Sphinx 9 but will
  need an upgrade before moving to Sphinx 10.
- `sphinx-book-theme` 1.2.0 is in maintenance mode (bug-fixes only); it has no
  upper Sphinx bound and works fine with Sphinx 9.
- `make html` currently produces 8 pre-existing content warnings (broken
  toctree refs / orphan documents) unrelated to the Sphinx version.

Some included utils extensions:
  - [ablog](https://ablog.readthedocs.io/en/stable/)
  - [myst_nb](https://myst-nb.readthedocs.io/en/latest/)
  - [numpydoc](https://numpydoc.readthedocs.io/en/latest/format.html) 
  - [matplotlib](https://matplotlib.org/)
  - [sphinx_design](https://sphinx-design.readthedocs.io/en/latest/)
  - [sphinx_copybutton](https://sphinx-copybutton.readthedocs.io/en/latest/)
  - [sphinx_togglebutton](https://sphinx-togglebutton.readthedocs.io/en/latest/)
  - [sphinxext.opengraph](https://sphinxext-opengraph.readthedocs.io/en/latest/)
  - [sphinxcontrib.youtube](https://sphinxcontrib-youtube.readthedocs.io/en/latest/)
  - [sphinx_thebe](https://sphinx-thebe.readthedocs.io/en/latest/)
  - [sphinxcontrib.bibtex](https://sphinxcontrib-bibtex.readthedocs.io/en/latest/)
  - [sphinxcontrib.mermaid](https://sphinxcontrib-mermaid-demo.readthedocs.io/en/latest/)

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
[MyST Markdown]: https://mystmd.org/guide
