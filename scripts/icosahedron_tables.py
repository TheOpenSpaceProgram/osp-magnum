# Open Space Program
# Copyright © 2019-2024 Open Space Program Project
#
# MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from mpmath import *

mp.prec = 80 # overkill precision

def nstr_fixed(value, sigfigs):
    value = mpmathify(value);
    if value == 0.0:
        # mpmath would intentionally output 0.0e+0 otherwise for some reason
        return " 0.000000000000000e+0"
    return (" " if value > 0 else "") + nstr(mpmathify(value), sigfigs, min_fixed=1, max_fixed=0, show_zero_exponent=True, strip_zeros=False);

def nstr_double(value):
    return nstr_fixed(value, 16)

def nstr_float(value):
    return nstr_fixed(value, 9) + "f"

def mid(a, b):
    return (0.5*(a[0]+b[0]), 0.5*(a[1]+b[1]), 0.5*(a[2]+b[2]))

def mag(a):
    return sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2])

def norm(a):
    m = mag(a)
    return (a[0]/m, a[1]/m, a[2]/m)

def distance(a, b):
    return mag((b[0]-a[0], b[1]-a[1], b[2]-a[2]))

# -----------------------------------------------------------------------------

# Create an Icosahedron. Blender style, so there's a vertex directly on
# top and directly on the bottom. Basicly, a sandwich of two pentagons,
# rotated 180 apart from each other, and each 1/sqrt(5) above and below
# the origin.
#
# Icosahedron indices viewed from above (Z)
#
#          5
#  4
#
#        0      1
#
#  3
#          2
#
# Useful page from wolfram:
# https://mathworld.wolfram.com/RegularPentagon.html
#
# The 'radius' of the pentagons are NOT 1.0, as they are slightly above or
# below the origin. They have to be slightly smaller to keep their
# distance to the 3D origin as 1.0.
#
# it works out to be (2/5 * sqrt(5)) ~~ 90% the size of a typical pentagon
#
# Equations 5..8 from the wolfram page:
# c1 = 1/4 * ( sqrt(5) - 1 )
# c2 = 1/4 * ( sqrt(5) + 1 )
# s1 = 1/4 * sqrt( 10 + 2*sqrt(5) )
# s2 = 1/4 * sqrt( 10 - 2*sqrt(5) )
#
# Now multiply by (2/5 * sqrt(5)), using auto-simplify
# let m = (2/5 * sqrt(5))
# cxA = m * c1 = 1/2 - sqrt(5)/10
# cxB = m * c2 = 1/2 + sqrt(5)/10
# syA = m * s1 = 1/10 * sqrt( 10 * (5 + sqrt(5)) )
# syN = m * s2 = 1/10 * sqrt( 10 * (5 - sqrt(5)) )

# Height of pentagon within the icosahedron
hei = 1.0/sqrt(5.0)

# Circumcircle radius of pentagon
cmr = 2.0/5.0 * sqrt(5.0)

# X and Y coordinates used to construct pentagons
cxa = 1.0/2.0 - sqrt(5.0)/10.0
cxb = 1.0/2.0 + sqrt(5)/10.0
sya = 1.0/10.0 * sqrt( 10.0 * (5.0+sqrt(5.0)) )
syb = 1.0/10.0 * sqrt( 10.0 * (5.0-sqrt(5.0)) )

icosahedron_vrtx = [
    ( 0.0,    0.0,    1.0), # 0 top point
    ( cmr,    0.0,    hei), # 1 top pentagon
    ( cxa,   -sya,    hei), # 2
    (-cxb,   -syb,    hei), # 3
    (-cxb,    syb,    hei), # 4
    ( cxa,    sya,    hei), # 5
    (-cmr,    0.0,   -hei), # 6 bottom pentagon
    (-cxa,   -sya,   -hei), # 7
    ( cxb,   -syb,   -hei), # 8
    ( cxb,    syb,   -hei), # 9
    (-cxa,    sya,   -hei), # 10
    ( 0.0,    0.0,   -1.0)  # 11 bottom point
];

print("inline constexpr std::array<osp::Vector3d, 12> gc_icoVrtxPos\n"
       + "{{\n"
       + ",\n".join("    {"+nstr_double(x)+", "+nstr_double(y)+", "+nstr_double(z)+"}" for (x, y, z) in icosahedron_vrtx) + "\n"
       + "}};")

# -----------------------------------------------------------------------------

# Edge lengths vs subdiv level

edge_length_min = [2.0*sin(0.5*acot(0.5) * 0.5**x) for x in range(0, 24)]

print("inline constexpr std::array<float, 24> const gc_icoMinEdgeVsSubdiv\n"
      + "{\n"
      + "    " + ", ".join(nstr_float(edge_length) for edge_length in edge_length_min) + "\n"
      + "};\n")


edge_length_max = []

# Any initial triangle from the icosahedron works here
tri = (
    ( 0.0,    0.0,    1.0),
    ( cmr,    0.0,    hei),
    ( cxa,   -sya,    hei)
)

for _ in range(0, 24):

    edge_length_max.append(distance(tri[0], tri[1]))

    # Middle triangle is the largest, so it's the maximum
    tri = (
        norm(mid(tri[0], tri[1])),
        norm(mid(tri[1], tri[2])),
        norm(mid(tri[2], tri[0]))
    )

print("inline constexpr std::array<float, 24> const gc_icoMaxEdgeVsSubdiv\n"
      + "{\n"
      + "    " + ", ".join(nstr_float(edge_length) for edge_length in edge_length_max) + "\n"
      + "};\n")

# -----------------------------------------------------------------------------

# Tower height
# If identical towers were built on the two vertices spanning an edge, this is how high each tower
# needs to be in order to see each other over the horizon.
#  = exsec(0.5*acot(0.5) * levels)
tower_heights = [sec(0.5*acot(0.5) * 0.5**x ) - 1 for x in range(0, 24)]

print("inline constexpr std::array<float, 24> const gc_icoTowerOverHorizonVsSubdiv\n"
      + "{\n"
      + "    " + ", ".join(nstr_float(edge_length) for edge_length in tower_heights) + "\n"
      + "};\n")
