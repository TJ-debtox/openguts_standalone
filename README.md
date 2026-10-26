# openguts_standalone
This repository is the archive for the openGUTS standalone (Windows installer and source code). This archives version 1.2 of 20 March 2024.

## ⚠️ Project Status: Archived

**This repository has been archived following the closure of business operations at DEBtox Research and is no longer actively maintained. The code is provided strictly "as-is".**

This repository only concerns version 1.2 of openGUTS. I expect there to be updates of openGUTS in the future, but these will not be offered or archived here (under TJ-debtox). This archive will likely be removed when a new version is released; the openGUTS website ([https://www.openguts.info/](https://www.openguts.info/)) will always link to the most recent version.

### Bug Reports & Security Warnings
Since this repository is locked (read-only), new issues or pull requests cannot be opened. However, if a critical bug or error is discovered in these files in the future, a warning notice will be posted here:

* **Future updates/warnings will be shared via:** [https://www.openguts.info/download.html](https://www.openguts.info/download.html)
* You can reach me at: **tjalling [at] debtox [dot] nl** (which I will try to keep active) or [via LinkedIn](https://nl.linkedin.com/pub/tjalling-jager/5/906/147)


## Licensing

The openGUTS project as a whole is licensed under the **GNU General Public License v3.0** (GPLv3) — see the [LICENSE](LICENSE) file in the root directory for the full terms. 

### Authors

The C++ source code for openGUTS version 1 was written by [WSC Scientific GmbH](http://wsc-regexperts.com/en/), using Matlab code written by Tjalling Jager (DEBtox Research) as a prototype. Versions 1.1 and 1.2 of the C++ source code was developed by [RIFCON GmbH](https://www.rifcon.com/en/), based on the code for version 1, and the modifications to the Matlab code as made by Tjalling Jager. Exceptions are the third-party C++ functions listed below.

The Matlab code for version 1.2 can be found at [https://github.com/TJ-debtox/openguts_archive/](https://github.com/TJ-debtox/openguts_archive/).

### Third-party licenses

Please note that that the third-party code for the docto pdf converter is included under an MIT license, and the Nelder-Mead optimisation is licensed under the GNU LGPL license. Each third-party file is included in the folder [src-pkg_1.2\License information\](src-pkg_1.2\License information\).

## What is openGUTS?

OpenGUTS is the implementation of the General Unified Threshold model for Survival (GUTS) into software. GUTS is a toxicokinetic-toxicodynamic (TKTD) model for the endpoint survival. For more information, see the supporting web site (see link below).

OpenGUTS is available as a freely-downloadable, open source, standalone for Windows, and as a set of Matlab scripts and functions. This repository is for the standalone version and contains the Windows installer and C++ source code. The Matlab version can be found at [https://github.com/TJ-debtox/openguts_archive/](https://github.com/TJ-debtox/openguts_archive/).

The Matlab version has the same functionality as the standalone version, but lacks the user interface and the option to make formatted output reports. Both versions are able to use the same input files for the data sets and the exposure profiles. The Matlab version will be handy as a platform for modification and experimentation. Furthermore, since the Matlab version was originally developed as prototype for the C++ source code, both follow the same structure, and the Matlab version can be used for future development of the software. 

### About this archive

As of 1 December 2026, I (Tjalling Jager) have retired my business DEBtox Research. This archive on GitHub contains the last release that I have contributed to: version 1.2 of 20 March 2024. Future updates will be hosted elsewhere (see supporting website for the latest version and the correct download link).

### Supporting web site

The supporting web site for openGUTS is [https://www.openguts.info/](https://www.openguts.info/). This website no longer contains the downloads for the standalone version, but still hosts manuals, background documents, description of openGUTS, etc. This page will also collect bugs, errors, and other issues that may arise after archiving the repository on GitHub.

## Installation and getting started

To install the openGUTS standalone version (Windows 7 and later), you only need to download the installer `Setup_openGUTS_1_2.msi`. Before installing this version, make sure to uninstall any previous version of openGUTS from your PC. For more information on the installation and how to get started with openGUTS, please read the manual provided at [https://www.openguts.info/download.html](https://www.openguts.info/download.html)/

