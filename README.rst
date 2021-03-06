=======
Mahotas
=======
Python Computer Vision Library
------------------------------

This library of fast computer vision algorithms (all implemented in C++)
operates over numpy arrays for convenience.

Notable algorithms:
 - watershed.
 - convex points calculations.
 - hit & miss. thinning.
 - Zernike & Haralick, LBP, and TAS features.
 - freeimage based numpy image loading (requires freeimage libraries to be
   installed).
 - Speeded-Up Robust Features (SURF), a form of local features.
 - thresholding.
 - convolution.
 - Sobel edge detection.
 - spline interpolation

Examples
--------

This is a simple example of loading a file (called `test.jpeg`) and calling
`watershed` using above threshold regions as a seed (we use Otsu to define
threshold).

::

    import numpy as np
    from scipy import ndimage
    import mahotas
    import pylab

    img = mahotas.imread('test.jpeg')
    T_otsu = mahotas.thresholding.otsu(img)
    seeds,_ = ndimage.label(img > T_otsu)
    labeled = mahotas.cwatershed(img.max() - img, seeds)

Here is a very simple example of using ``mahotas.distance`` (which computes a
distance map)::

    import pylab as p
    import numpy as np
    import mahotas

    f = np.ones((256,256), bool)
    f[200:,240:] = False
    f[128:144,32:48] = False
    # f is basically True with the exception of two islands: one in the lower-right
    # corner, another, middle-left

    dmap = mahotas.distance(f)
    p.imshow(dmap)
    p.show()

(This is under ``mahotas/demos/distance.py``).

How to invoke thresholding functions::

    import mahotas
    import numpy as np
    from pylab import imshow, gray, show, subplot
    from os import path

    photo = mahotas.imread('luispedro.org', as_grey=True)
    photo = photo.astype(np.uint8)

    T_otsu = mahotas.otsu(photo)
    thresholded_otsu = (photo > T_otsu)

    T_rc = mahotas.rc(photo)
    thresholded_rc = (photo > T_rc)


Install
-------

You will need python (naturally), numpy, and a C++ compiler. Then you should be
able to either

Download the source and then run::

    python setup.py install

or use one of::

    pip install mahotas
    easy_install mahotas

You can test your instalation by running::

    python -c "import mahotas; mahotas.test()"


Contacts
--------

For bugfixes, feel free to use my email: luis@luispedro.org

For more general with achieving certain tasks in Python, the `pythonvision
mailing list <http://groups.google.com/group/pythonvision?pli=1>`__ is a much
better venue and generates a public discussion log for others in the future.

Recent Changes
--------------

0.9.2 (September 1 2012)
~~~~~~~~~~~~~~~~~~~~~~
- Fix compilation on Mac OS X 10.8 (reported by Davide Cittaro)
- Freeimage fixes on Windows by Christoph Gohlke
- Slightly faster _filter implementaiton


0.9.1 (August 28 2012)
~~~~~~~~~~~~~~~~~~~~~~

- Python 3 support (you need to use ``2to3``)
- Haar wavelets (forward and inverse transform)
- Daubechies wavelets (forward and inverse transform)
- Corner case fix in Otsu thresholding
- Add soft_threshold function
- Have polygon.convexhull return an ndarray (instead of a list)
- Memory usage improvements in regmin/regmax/close_holes (first reported
  as issue #9 by thanasi)


0.9 (July 16 2012)
~~~~~~~~~~~~~~~~~~
- Auto-convert integer to double on gaussian_filter (previously, integer
  values would result in zero-valued outputs).
- Check for integer types in (reg|loc)(max|min)
- Use name `out` instead of `output` for output arguments. This matches
  Numpy better
- Switched to MIT License

0.8.1 (June 6 2012)
~~~~~~~~~~~~~~~~~~~
- Fix gaussian_filter bug when order argument was used (reported by John Mark
  Agosta)
- Add morph.cerode
- Improve regmax() & regmin(). Rename previous implementations to locmax() &
  locmin()
- Fix erode() on non-contiguous arrays

0.8 (May 7 2012)
~~~~~~~~~~~~~~~~
- Move features to submodule
- Add morph.open function
- Add morph.regmax & morph.regmin functions
- Add morph.close function
- Fix morph.dilate crash

0.7.3 (March 14 2012)
~~~~~~~~~~~~~~~~~~~~~
- Fix installation of test data
- Greyscale erosion & dilation
- Use imread module (if available)
- Add output argument to erode() & dilate()
- Add 14th Haralick feature (patch by MattyG) --- currently off by default
- Improved zernike interface (zernike_moments)
- Add remove_bordering to labeled
- Faster implementation of ``bwperim``
- Add ``roundness`` shape feature


0.7.2 (February 13 2012)
~~~~~~~~~~~~~~~~~~~~~~~~

There were two minor additions:

- Add as_rgb (especially useful for interactive use)
- Add Gaussian filtering (from scipy.ndimage)

And a few bugfixes:

- Fix type bug in 32 bit machines (Bug report by Lech Wiktor Piotrowski)
- Fix convolve1d
- Fix rank_filter


0.7.1 (January 6 2012)
~~~~~~~~~~~~~~~~~~~~~~

The most important change fixed compilation on Mac OS X

Other changes:

- Add convolve1d
- Check that convolution arguments have right dimensions (instead of
  crashing)
- Add descriptor_only argument to surf.descriptors
- Specify all function signatures on freeimage.py


0.7 (December 5 2011)
~~~~~~~~~~~~~~~~~~~~~

The big change was that the *dependency on scipy was removed*. As part of this
process, the interpolate submodule was added. A few important bug fixes as
well.

- Allow specification of centre in Zernike moment computation
- Fix Local Binary Patterns
- Remove dependency on scipy
- Add interpolate module (from scipy.ndimage)
- Add labeled_sum & labeled_sizes
- gvoronoi no longer depends on scipy
- mahotas is importable without scipy
- Fix bugs in 2D TAS (reported by Jenn Bakal)
- Support for 1-bit monochrome image loading with freeimage
- Fix GIL handling on errors (reported by Gareth McCaughan)
- Fix freeimage for 64-bit computers

0.6.6 (August 8 2011)
~~~~~~~~~~~~~~~~~~~~~
- Fix ``fill_polygon`` bug (fix by joferkington)
- Fix Haralick feature 6 (fix by Rita Simões)
- Implement ``morph.get_structuring_element`` for ndim > 2. This implies that
  functions such as ``label()`` now also work in multiple dimensions
- Add median filter & ``rank_filter`` functions
- Add template_match function
- Refactor by use of mahotas.internal
- Better error message for when the compiled modules cannot be loaded
- Update contact email. All docs in numpydoc format now.

For version 0.6.5
~~~~~~~~~~~~~~~~~
- Add ``max_points`` & ``descriptor_only`` arguments to mahotas.surf
- Fix haralick for 3-D images (bug report by Rita Simões)
- Better error messages
- Fix hit&miss for non-boolean inputs
- Add ``label()`` function

For version 0.6.4
~~~~~~~~~~~~~~~~~

- Fix bug in ``cwatershed()`` when using return_lines=1
- Fix bug in ``cwatershed()`` when using equivalent types for image and markers
- Move tests to mahotas.tests and include them in distribution
- Include ChangeLog in distribution
- Fix compilation on the Mac OS
- Fix compilation warnings on gcc

For version 0.6.3
~~~~~~~~~~~~~~~~~
- Improve ``mahotas.stretch()`` function
- Fix corner case in surf (when determinant was zero)
- ``threshold`` argument in mahotas.surf
- imreadfromblob() & imsavetoblob() functions
- ``max_points`` argument for mahotas.surf.interest_points()
- Add ``mahotas.labeled.borders`` function

For version 0.6.2
~~~~~~~~~~~~~~~~~

Bugfix release:

- Fix memory leak in _surf
- More robust searching for freeimage
- More functions in mahotas.surf() to retrieve intermediate results
- Improve compilation on Windows (patches by Christoph Gohlke)

0.6.1 (Dec 13 2010)
~~~~~~~~~~~~~~~~~~~

- SURF local features
- Convolution
- mahotas.labeled functions
- just_filter option in edge.sobel()
- Release the GIL in morphological functions


0.6 (Nov 22 2010)
~~~~~~~~~~~~~~~~~

- Improve Local Binary patterns (faster and better interface)
- Much faster erode() (10x faster)
- Faster dilate() (2x faster)
- TAS for 3D images
- Haralick for 3D images
- Fix mahotas.imread for RGBA images


*Website*: `http://luispedro.org/software/mahotas
<http://luispedro.org/software/mahotas>`_

*API Docs*: `http://packages.python.org/mahotas/
<http://packages.python.org/mahotas/>`_

*Mailing List*: Use the `pythonvision mailing list
<http://groups.google.com/group/pythonvision?pli=1>`_ for questions, bug
submissions, etc.

*Author*: Luis Pedro Coelho (with code by Zachary Pincus [from scikits.image],
Peter J. Verveer [from scipy.ndimage], and Davis King [from dlib])

