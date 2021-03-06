// Copyright (C) 2008-2012, Luis Pedro Coelho <luis@luispedro.org>
// vim: set ts=4 sts=4 sw=4 expandtab smartindent:
// 
// License: MIT

#include <algorithm>
#include <queue>
#include <vector>
#include <cstdio>
#include <limits>
#include <iostream>

#include "numpypp/array.hpp"
#include "numpypp/dispatch.hpp"
#include "utils.hpp"

#include "_filters.h"

extern "C" {
    #include <Python.h>
    #include <numpy/ndarrayobject.h>
}

namespace{

const char TypeErrorMsg[] =
    "Type not understood. "
    "This is caused by either a direct call to _morph (which is dangerous: types are not checked!) or a bug in mahotas.\n";


template <typename T>
numpy::position central_position(const numpy::array_base<T>& array) {
    numpy::position res(array.raw_dims(), array.ndims());
    for (int i = 0, nd = array.ndims(); i != nd; ++i) res.position_[i] /= 2;
    return res;
}

template <typename T>
std::vector<numpy::position> neighbours(const numpy::aligned_array<T>& Bc, bool include_centre = false) {
    numpy::position centre = central_position(Bc);
    const unsigned N = Bc.size();
    typename numpy::aligned_array<T>::const_iterator startc = Bc.begin();
    std::vector<numpy::position> res;
    for (unsigned i = 0; i != N; ++i, ++startc) {
        if (!*startc) continue;
        if (startc.position() != centre || include_centre) {
            res.push_back(startc.position() - centre);
        }
    }
    return res;
}

template<typename T>
numpy::index_type margin_of(const numpy::position& position, const numpy::array_base<T>& ref) {
    numpy::index_type margin = std::numeric_limits<numpy::index_type>::max();
    for (int d = 0; d != ref.ndims(); ++d) {
        if (position[d] < margin) margin = position[d];
        int rmargin = ref.dim(d) - position[d] - 1;
        if (rmargin < margin) margin = rmargin;
   }
   return margin;
}

template<typename T>
T erode_sub(T a, T b) {
    if (b == std::numeric_limits<T>::min()) return std::numeric_limits<T>::max();
    const T r = a - b;
    if (!std::numeric_limits<T>::is_signed && (b > a)) return T(0);
    if ( std::numeric_limits<T>::is_signed && (r > a)) return std::numeric_limits<T>::min();
    return r;
}

template<>
bool erode_sub<bool>(bool a, bool b) {
    return a && b;
}

template<typename T> bool is_bool(T) { return false; }
template<> bool is_bool<bool>(bool) { return true; }

template<typename T>
void erode(numpy::aligned_array<T> res, numpy::aligned_array<T> array, numpy::aligned_array<T> Bc) {
    gil_release nogil;
    const int N = res.size();
    typename numpy::aligned_array<T>::iterator iter = array.begin();
    filter_iterator<T> filter(array.raw_array(), Bc.raw_array(), EXTEND_NEAREST, is_bool(T()));
    const int N2 = filter.size();
    T* rpos = res.data();

    for (int i = 0; i != N; ++i, ++rpos, filter.iterate_both(iter)) {
        T value = std::numeric_limits<T>::max();
        for (int j = 0; j != N2; ++j) {
            T arr_val = T();
            filter.retrieve(iter, j, arr_val);
            value = std::min<T>(value, erode_sub(arr_val, filter[j]));
        }
        *rpos = value;
    }
}


PyObject* py_erode(PyObject* self, PyObject* args) {
    PyArrayObject* array;
    PyArrayObject* Bc;
    PyArrayObject* output;
    if (!PyArg_ParseTuple(args, "OOO", &array, &Bc, &output)) return NULL;
    if (!numpy::are_arrays(array, Bc, output) || !numpy::same_shape(array, output) ||
        !numpy::equiv_typenums(array, Bc, output) ||
        PyArray_NDIM(array) != PyArray_NDIM(Bc)
    ) {
        PyErr_SetString(PyExc_RuntimeError, TypeErrorMsg);
        return NULL;
    }
    holdref r_o(output);

#define HANDLE(type) \
    erode<type>(numpy::aligned_array<type>(output), numpy::aligned_array<type>(array), numpy::aligned_array<type>(Bc));
    SAFE_SWITCH_ON_INTEGER_TYPES_OF(array, true);
#undef HANDLE

    Py_XINCREF(output);
    return PyArray_Return(output);
}

template<typename T>
void locmin_max(numpy::aligned_array<bool> res, numpy::aligned_array<T> array, numpy::aligned_array<T> Bc, bool is_min) {
    gil_release nogil;
    const int N = res.size();
    typename numpy::aligned_array<T>::iterator iter = array.begin();
    filter_iterator<T> filter(res.raw_array(), Bc.raw_array(), EXTEND_NEAREST, true);
    const int N2 = filter.size();
    bool* rpos = res.data();

    for (int i = 0; i != N; ++i, ++rpos, filter.iterate_both(iter)) {
        T cur = *iter;
        for (int j = 0; j != N2; ++j) {
            T arr_val = T();
            filter.retrieve(iter, j, arr_val);
            if (( is_min && (arr_val < cur)) ||
                (!is_min && (arr_val > cur))) {
                    goto skip_to_next;
                }
        }
        *rpos = true;
        skip_to_next:
            ;
    }
}

PyObject* py_locminmax(PyObject* self, PyObject* args) {
    PyArrayObject* array;
    PyArrayObject* Bc;
    PyArrayObject* output;
    int is_min;
    if (!PyArg_ParseTuple(args, "OOOi", &array, &Bc, &output, &is_min)) return NULL;
    if (!numpy::are_arrays(array, Bc, output) || !numpy::same_shape(array, output) ||
        !PyArray_EquivTypenums(PyArray_TYPE(array), PyArray_TYPE(Bc)) ||
        !PyArray_EquivTypenums(NPY_BOOL, PyArray_TYPE(output)) ||
        PyArray_NDIM(array) != PyArray_NDIM(Bc) ||
        !PyArray_ISCARRAY(output)
    ) {
        PyErr_SetString(PyExc_RuntimeError, TypeErrorMsg);
        return NULL;
    }
    holdref r_o(output);
    PyArray_FILLWBYTE(output, 0);

#define HANDLE(type) \
    locmin_max<type>(numpy::aligned_array<bool>(output), numpy::aligned_array<type>(array), numpy::aligned_array<type>(Bc), bool(is_min));
    SAFE_SWITCH_ON_INTEGER_TYPES_OF(array, true);
#undef HANDLE

    Py_XINCREF(output);
    return PyArray_Return(output);
}

template <typename T>
void remove_fake_regmin_max(numpy::aligned_array<bool> regmin, numpy::aligned_array<T> f, numpy::aligned_array<T> Bc, bool is_min) {
    const int N = f.size();
    numpy::aligned_array<bool>::iterator riter = regmin.begin();
    const std::vector<numpy::position> Bc_neighbours = neighbours(Bc);
    typedef std::vector<numpy::position>::const_iterator Bc_iter;
    const int N2 = Bc_neighbours.size();

    for (int i = 0; i != N; ++i, ++riter) {
        if (!*riter) continue;
        const numpy::position pos = riter.position();
        const T val = f.at(pos);
        for (int j = 0; j != N2; ++j) {
            numpy::position npos = pos + Bc_neighbours[j];
            if (f.validposition(npos) &&
                    !regmin.at(npos) &&
                            (   (is_min && f.at(npos) <= val) ||
                                (!is_min && f.at(npos) >= val)
                            )) {
                numpy::position_stack stack(f.ndims());
                assert(regmin.at(pos));
                regmin.at(pos) = false;
                stack.push(pos);
                while (!stack.empty()) {
                    numpy::position p = stack.top_pop();
                    for (Bc_iter first = Bc_neighbours.begin(), past = Bc_neighbours.end();
                                first != past;
                                ++first) {
                        numpy::position npos = p + *first;
                        if (regmin.validposition(npos) && regmin.at(npos)) {
                            regmin.at(npos) = false;
                            assert(!regmin.at(npos));
                            stack.push(npos);
                        }
                    }
                }
                // we are done with this position
                break;
            }
        }
    }
}

PyObject* py_regminmax(PyObject* self, PyObject* args) {
    PyArrayObject* array;
    PyArrayObject* Bc;
    PyArrayObject* output;
    int is_min;
    if (!PyArg_ParseTuple(args, "OOOi", &array, &Bc, &output, &is_min)) return NULL;
    if (!numpy::are_arrays(array, Bc, output) || !numpy::same_shape(array, output) ||
        !PyArray_EquivTypenums(PyArray_TYPE(array), PyArray_TYPE(Bc)) ||
        !PyArray_EquivTypenums(NPY_BOOL, PyArray_TYPE(output)) ||
        PyArray_NDIM(array) != PyArray_NDIM(Bc) ||
        !PyArray_ISCARRAY(output)
    ) {
        PyErr_SetString(PyExc_RuntimeError, TypeErrorMsg);
        return NULL;
    }
    holdref r_o(output);
    PyArray_FILLWBYTE(output, 0);

#define HANDLE(type) \
    locmin_max<type>(numpy::aligned_array<bool>(output), numpy::aligned_array<type>(array), numpy::aligned_array<type>(Bc), bool(is_min)); \
    remove_fake_regmin_max<type>(numpy::aligned_array<bool>(output), numpy::aligned_array<type>(array), numpy::aligned_array<type>(Bc), bool(is_min));

    SAFE_SWITCH_ON_INTEGER_TYPES_OF(array, true);
#undef HANDLE

    Py_XINCREF(output);
    return PyArray_Return(output);
}



template <typename T>
T dilate_add(T a, T b) {
    if (a == std::numeric_limits<T>::min()) return a;
    if (b == std::numeric_limits<T>::min()) return b;
    const T r = a + b;
    // if overflow, saturate
    if (r < std::max<T>(a,b)) return std::numeric_limits<T>::max();
    return r;
}

template<>
bool dilate_add(bool a, bool b) {
    return a && b;
}

template<typename T>
void dilate(numpy::aligned_array<T> res, numpy::array<T> array, numpy::aligned_array<T> Bc) {
    gil_release nogil;
    const int N = res.size();
    typename numpy::array<T>::iterator iter = array.begin();
    filter_iterator<T> filter(res.raw_array(), Bc.raw_array(), EXTEND_NEAREST, is_bool(T()));
    const int N2 = filter.size();
    // T* is a fine iterator type.
    T* rpos = res.data();
    std::fill(rpos, rpos + res.size(), std::numeric_limits<T>::min());

    for (int i = 0; i != N; ++i, ++rpos, filter.iterate_both(iter)) {
        const T value = *iter;
        for (int j = 0; j != N2; ++j) {
            const T nval = dilate_add(value, filter[j]);
            T arr_val = T();
            filter.retrieve(rpos, j, arr_val);
            if (nval > arr_val) filter.set(rpos, j, nval);
        }
    }
}


PyObject* py_dilate(PyObject* self, PyObject* args) {
    PyArrayObject* array;
    PyArrayObject* Bc;
    PyArrayObject* output;
    if (!PyArg_ParseTuple(args,"OOO", &array, &Bc, &output)) return NULL;
    if (!numpy::are_arrays(array, Bc, output) || !numpy::same_shape(array, output) ||
        !PyArray_EquivTypenums(PyArray_TYPE(array), PyArray_TYPE(Bc)) ||
        !PyArray_EquivTypenums(PyArray_TYPE(array), PyArray_TYPE(output)) ||
        PyArray_NDIM(array) != PyArray_NDIM(Bc)
    ) {
        PyErr_SetString(PyExc_RuntimeError, TypeErrorMsg);
        return NULL;
    }
    holdref r_o(output);
#define HANDLE(type) \
    dilate<type>(numpy::aligned_array<type>(output),numpy::array<type>(array),numpy::aligned_array<type>(Bc));
    SAFE_SWITCH_ON_INTEGER_TYPES_OF(array, true);
#undef HANDLE

    Py_XINCREF(output);
    return PyArray_Return(output);
}

void close_holes(numpy::aligned_array<bool> ref, numpy::aligned_array<bool> f, numpy::aligned_array<bool> Bc) {
    std::fill_n(f.data(),f. size(), false);

    numpy::position_stack stack(ref.ndim());
    const int N = ref.size();
    const std::vector<numpy::position> Bc_neighbours = neighbours(Bc);
    const int N2 = Bc_neighbours.size();
    for (int d = 0; d != ref.ndims(); ++d) {
        if (ref.dim(d) == 0) continue;
        numpy::position pos;
        pos.nd_ = ref.ndims();
        for (int di = 0; di != ref.ndims(); ++di) pos.position_[di] = 0;

        for (int i = 0; i != N/ref.dim(d); ++i) {
            pos.position_[d] = 0;
            if (!ref.at(pos) && !f.at(pos)) {
                f.at(pos) = true;
                stack.push(pos);
            }
            pos.position_[d] = ref.dim(d) - 1;
            if (!ref.at(pos) && !f.at(pos)) {
                f.at(pos) = true;
                stack.push(pos);
            }

            for (int j = 0; j != ref.ndims() - 1; ++j) {
                if (j == d) ++j;
                if (pos.position_[j] < int(ref.dim(j))) {
                    ++pos.position_[j];
                    break;
                }
                pos.position_[j] = 0;
            }
        }
    }
    while (!stack.empty()) {
        numpy::position pos = stack.top_pop();
        std::vector<numpy::position>::const_iterator startc = Bc_neighbours.begin();
        for (int j = 0; j != N2; ++j, ++startc) {
            numpy::position npos = pos + *startc;
            if (ref.validposition(npos) && !ref.at(npos) && !f.at(npos)) {
                f.at(npos) = true;
                stack.push(npos);
            }
        }
    }
    for (bool* start = f.data(), *past = f.data() + f.size(); start != past; ++ start) {
        *start = !*start;
    }
}

PyObject* py_close_holes(PyObject* self, PyObject* args) {
    PyArrayObject* ref;
    PyArrayObject* Bc;
    if (!PyArg_ParseTuple(args,"OO", &ref, &Bc)) {
        return NULL;
    }
    PyArrayObject* res_a = (PyArrayObject*)PyArray_SimpleNew(ref->nd,ref->dimensions,PyArray_TYPE(ref));
    if (!res_a) return NULL;
    if (PyArray_TYPE(ref) != NPY_BOOL || PyArray_TYPE(Bc) != NPY_BOOL) {
        PyErr_SetString(PyExc_RuntimeError,TypeErrorMsg);
        return NULL;
    }
    try {
        close_holes(numpy::aligned_array<bool>(ref), numpy::aligned_array<bool>(res_a), numpy::aligned_array<bool>(Bc));
    }
    CATCH_PYTHON_EXCEPTIONS(false)
    if (PyErr_Occurred()) {
        Py_DECREF(res_a);
        return NULL;
    }
    return PyArray_Return(res_a);
}

struct MarkerInfo {
    int cost;
    int idx;
    int position;
    int margin;
    MarkerInfo(int cost, int idx, int position, int margin)
        :cost(cost)
        ,idx(idx)
        ,position(position)
        ,margin(margin) {
        }
    bool operator < (const MarkerInfo& other) const {
        // We want the smaller argument to compare higher, so we reverse the order here:
        if (cost == other.cost) return idx > other.idx;
        return cost > other.cost;
    }
};

struct NeighbourElem {
    NeighbourElem(int delta, int margin, const numpy::position& delta_position)
        :delta(delta)
        ,margin(margin)
        ,delta_position(delta_position)
        { }
    int delta;
    int margin;
    numpy::position delta_position;
};

template<typename BaseType>
void cwatershed(numpy::aligned_array<BaseType> res, numpy::aligned_array<bool>* lines, numpy::aligned_array<BaseType> array, numpy::aligned_array<BaseType> markers, numpy::aligned_array<BaseType> Bc) {
    gil_release nogil;
    const int N = res.size();
    const int N2 = Bc.size();
    std::vector<NeighbourElem> neighbours;
    const numpy::position centre = central_position(Bc);
    typename numpy::aligned_array<BaseType>::iterator Bi = Bc.begin();
    for (int j = 0; j != N2; ++j, ++Bi) {
        if (*Bi) {
            numpy::position npos = Bi.position() - centre;
            int margin = 0;
            for (int d = 0; d != Bc.ndims(); ++d) {
                margin = std::max<int>(std::abs(int(npos[d])), margin);
            }
            int delta = markers.pos_to_flat(npos);
            if (!delta) continue;
            neighbours.push_back(NeighbourElem(delta, margin, npos));
        }
    }
    int idx = 0;

    std::vector<BaseType> cost(array.size());
    std::fill(cost.begin(), cost.end(), std::numeric_limits<BaseType>::max());

    std::vector<bool> status(array.size());
    std::fill(status.begin(), status.end(),false);

    std::priority_queue<MarkerInfo> hqueue;

    typename numpy::aligned_array<BaseType>::iterator mpos = markers.begin();
    for (int i =0; i != N; ++i, ++mpos) {
        if (*mpos) {
            assert(markers.validposition(mpos.position()));
            int margin = markers.size();
            for (int d = 0; d != markers.ndims(); ++d) {
                if (mpos.index(d) < margin) margin = mpos.index(d);
                int rmargin = markers.dim(d) - mpos.index(d) - 1;
                if (rmargin < margin) margin = rmargin;
            }
            hqueue.push(MarkerInfo(array.at(mpos.position()), idx++, markers.pos_to_flat(mpos.position()), margin));
            res.at(mpos.position()) = *mpos;
            cost[markers.pos_to_flat(mpos.position())] = array.at(mpos.position());
        }
    }

    while (!hqueue.empty()) {
        MarkerInfo next = hqueue.top();
        hqueue.pop();
        if (status[next.position]) continue;
        status[next.position] = true;
        for (std::vector<NeighbourElem>::const_iterator neighbour = neighbours.begin(), past = neighbours.end(); neighbour != past; ++neighbour) {
            numpy::index_type npos = next.position + neighbour->delta;
            int nmargin = next.margin - neighbour->margin;
            if (nmargin < 0) {
                numpy::position pos = markers.flat_to_pos(next.position);
                assert(markers.validposition(pos));
                numpy::position npos = pos + neighbour->delta_position;
                if (!markers.validposition(npos)) continue;


                // we are good, but the margin might have been wrong. Recompute
                nmargin = margin_of(npos, markers);
            }
            assert(npos < int(cost.size()));
            if (!status[npos]) {
                BaseType ncost = array.at_flat(npos);
                if (ncost < cost[npos]) {
                    cost[npos] = ncost;
                    res.at_flat(npos) = res.at_flat(next.position);
                    hqueue.push(MarkerInfo(ncost, idx++, npos, nmargin));
                } else if (lines && res.at_flat(next.position) != res.at_flat(npos) && !lines->at_flat(npos)) {
                    lines->at_flat(npos) = true;
                }
            }
        }
    }
}

PyObject* py_cwatershed(PyObject* self, PyObject* args) {
    PyArrayObject* array;
    PyArrayObject* markers;
    PyArrayObject* Bc;
    int return_lines;
    if (!PyArg_ParseTuple(args,"OOOi", &array, &markers, &Bc,&return_lines)) {
        return NULL;
    }
    if (!PyArray_EquivTypenums(PyArray_TYPE(array), PyArray_TYPE(markers))) {
        PyErr_SetString(PyExc_RuntimeError, "mahotas._cwatershed: markers and f should have equivalent types.");
        return NULL;
    }
    PyArrayObject* res_a = (PyArrayObject*)PyArray_SimpleNew(array->nd,array->dimensions,PyArray_TYPE(array));
    if (!res_a) return NULL;
    PyArrayObject* lines =  0;
    numpy::aligned_array<bool>* lines_a = 0;
    if (return_lines) {
        lines = (PyArrayObject*)PyArray_SimpleNew(array->nd, array->dimensions, NPY_BOOL);
        if (!lines) return NULL;
        lines_a = new numpy::aligned_array<bool>(lines);
    }
#define HANDLE(type) \
    cwatershed<type>(numpy::aligned_array<type>(res_a),lines_a,numpy::aligned_array<type>(array),numpy::aligned_array<type>(markers),numpy::aligned_array<type>(Bc));
    SAFE_SWITCH_ON_INTEGER_TYPES_OF(array,true)
#undef HANDLE
    if (return_lines) {
        delete lines_a;
        PyObject* ret_val = PyTuple_New(2);
        PyTuple_SetItem(ret_val,0,(PyObject*)res_a);
        PyTuple_SetItem(ret_val,1,(PyObject*)lines);
        return ret_val;
    }
    return PyArray_Return(res_a);
}

struct HitMissNeighbour {
    HitMissNeighbour(int delta, int value)
        :delta(delta)
        ,value(value)
        { }
    int delta;
    int value;
};

template <typename T>
void hitmiss(numpy::aligned_array<T> res, const numpy::aligned_array<T>& input, const numpy::aligned_array<T>& Bc) {
    gil_release nogil;
    typedef typename numpy::aligned_array<T>::iterator iterator;
    typedef typename numpy::aligned_array<T>::const_iterator const_iterator;
    const numpy::index_type N = input.size();
    const numpy::index_type N2 = Bc.size();
    const numpy::position centre = central_position(Bc);
    int Bc_margin = 0;
    for (int d = 0; d != Bc.ndims(); ++d) {
        int cmargin = Bc.dim(d)/2;
        if (cmargin > Bc_margin) Bc_margin = cmargin;
    }

    std::vector<HitMissNeighbour> neighbours;
    const_iterator Bi = Bc.begin();
    for (int j = 0; j != N2; ++j, ++Bi) {
        if (*Bi != 2) {
            numpy::position npos = Bi.position() - centre;
            int delta = input.pos_to_flat(npos);
            neighbours.push_back(HitMissNeighbour(delta, *Bi));
        }
    }

    // This is a micro-optimisation for templates with structure
    // It makes it more likely that matching will fail earlier
    // in uniform regions than otherwise would be the case.
    std::random_shuffle(neighbours.begin(), neighbours.end());
    int slack = 0;
    for (int i = 0; i != N; ++i) {
        while (!slack) {
            numpy::position cur = input.flat_to_pos(i);
            bool moved = false;
            for (int d = 0; d != input.ndims(); ++d) {
                int margin = std::min<int>(cur[d], input.dim(d) - cur[d] - 1);
                if (margin < Bc.dim(d)/2) {
                    int size = 1;
                    for (int dd = d+1; dd < input.ndims(); ++dd) size *= input.dim(dd);
                    for (int j = 0; j != size; ++j) {
                        res.at_flat(i++) = 0;
                        if (i == N) return;
                    }
                    moved = true;
                    break;
                }
            }
            if (!moved) slack = input.dim(input.ndims() - 1) - Bc.dim(input.ndims() - 1) + 1;
        }
        --slack;
        T value = 1;
        for (std::vector<HitMissNeighbour>::const_iterator neighbour = neighbours.begin(), past = neighbours.end();
            neighbour != past;
            ++neighbour) {
            if (input.at_flat(i + neighbour->delta) != static_cast<T>(neighbour->value)) {
                value = 0;
                break;
            }
        }
        res.at_flat(i) = value;
    }
}

PyObject* py_hitmiss(PyObject* self, PyObject* args) {
    PyArrayObject* array;
    PyArrayObject* Bc;
    PyArrayObject* res_a;
    if (!PyArg_ParseTuple(args, "OOO", &array, &Bc, &res_a)) {
        PyErr_SetString(PyExc_RuntimeError,TypeErrorMsg);
        return NULL;
    }
    holdref r(res_a);

#define HANDLE(type) \
    hitmiss<type>(numpy::aligned_array<type>(res_a), numpy::aligned_array<type>(array), numpy::aligned_array<type>(Bc));
    SAFE_SWITCH_ON_INTEGER_TYPES_OF(array, true);
#undef HANDLE

    Py_INCREF(res_a);
    return PyArray_Return(res_a);
}

PyObject* py_majority_filter(PyObject* self, PyObject* args) {
    PyArrayObject* array;
    PyArrayObject* res_a;
    int N;
    if (!PyArg_ParseTuple(args, "OiO", &array, &N, &res_a) ||
        !PyArray_Check(array) || !PyArray_Check(res_a) ||
        PyArray_TYPE(array) != NPY_BOOL || PyArray_TYPE(res_a) != NPY_BOOL ||
        !PyArray_ISCARRAY(res_a)) {
        PyErr_SetString(PyExc_RuntimeError,TypeErrorMsg);
        return NULL;
    }
    Py_INCREF(res_a);
    PyArray_FILLWBYTE(res_a, 0);
    numpy::aligned_array<bool> input(array);
    numpy::aligned_array<bool> output(res_a);
    const int rows = input.dim(0);
    const int cols = input.dim(1);
    const int T = N*N/2;
    if (rows < N || cols < N) {
        return PyArray_Return(res_a);
    }
    for (int y = 0; y != rows-N; ++y) {
        bool* output_iter = output.data() + (y+int(N/2)) * output.stride(0) + int(N/2);
        for (int x = 0; x != cols-N; ++x) {
            int count = 0;
            for (int dy = 0; dy != N; ++dy) {
                for (int dx = 0; dx != N; ++dx) {
                    if (input.at(y+dy,x+dx)) ++count;
                }
            }
            if (count >= T) {
                *output_iter = true;
            }
            ++output_iter;
        }
    }
    return PyArray_Return(res_a);
}

PyMethodDef methods[] = {
  {"dilate",(PyCFunction)py_dilate, METH_VARARGS, NULL},
  {"erode",(PyCFunction)py_erode, METH_VARARGS, NULL},
  {"close_holes",(PyCFunction)py_close_holes, METH_VARARGS, NULL},
  {"cwatershed",(PyCFunction)py_cwatershed, METH_VARARGS, NULL},
  {"locmin_max",(PyCFunction)py_locminmax, METH_VARARGS, NULL},
  {"regmin_max",(PyCFunction)py_regminmax, METH_VARARGS, NULL},
  {"hitmiss",(PyCFunction)py_hitmiss, METH_VARARGS, NULL},
  {"majority_filter",(PyCFunction)py_majority_filter, METH_VARARGS, NULL},
  {NULL, NULL,0,NULL},
};

} // namespace

DECLARE_MODULE(_morph)
