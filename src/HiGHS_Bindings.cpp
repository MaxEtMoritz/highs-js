#include <emscripten/bind.h>
#include <Highs.h>
#include <type_traits>

namespace py = emscripten;
#define def function
#define def_readwrite property
#define def_property_readonly property

/*
    Things to do manually:
    1. copy contents inside the PYBIND11_MODULE block of HiGHS/src/highs_bindings.cpp into the EMSCRIPTEN_BINDINGS block into the #pragma region PYBIND11_MODULE content here
    2. copy the file contents before PYBIND11_MODULE (starting after the #includes and namespace definitions) into the #pragma region Preamble here
    2. some differences in function naming between pybind11 and embind are redefined by the #defines above and do not need to be changed
    3. other things like different set of parameters cannot be easily redefined by preprocessor directive and need to be manually adapted:
        - for enum definitions:
            - remove the first 'm' parameter (replace "m,\s*" with "")
            - delete or uncomment call to  if it is present (replace "\.export_values\(\)" with "")
        - for class definitions:
            - change constructor definitions (replace "\.def\(py::init<(.*)>\(\)\)" with ".constructor<$1>()")
        - for constants definitions:
            - change how constants are defined (replace "m\.attr\("(\w+)"\) = (\w+);" with "emscripten::constant("$1", $2);")
        - for submodule definitions:
            - resolve manually: embind does not support mapping namespaces yet (see https://github.com/emscripten-core/emscripten/issues/15474)
                --> remove definition of submodule, remove first param from members of the submodule, change name of bound symbols e.g. from
                class Logger in namespace a to class a_Logger
        - for the preamble:
            - replace dense_array_t<double> with DoubleArray
            - replace dense_array_t<HighsInt> with IntArray
            - replace dense_array_t<HighsVarType> with HighsVarTypeArray
            - replace "py::buffer_info\s*(.+)[\s\n]*=[\s\n]*(.+)\.request\(\);" with "auto $1 = emscripten::vecFromJSArray<type>($2);"
            - manually replace "type" in all previously replaced occurrences with the correct one (i.e. HighsInt for IntArray, double for DoubleArray)
            - replace "\.ptr" with ".data()"
            - replace "(.*)std::tuple<((?:.|\n)+?)>[\s\n/]*(.+)\((.*)" with "$1EMSCRIPTEN_DECLARE_VAL_TYPE($3Result)\n$1$3Result $3($4"
            - replace "(?<=EMSCRIPTEN_DECLARE_VAL_TYPE\((.+?)\)(?:.|\n)*?)return std::make_tuple\(((?:.|\n)+?)\);" with "return make_JsArray<$1>($2);"
            - replace "Highs\s*?\*\s*?h" with "Highs& h" (otherwise emscripten complains about use of raw pointers without setting a policy)
            - replace "(?<=\s)h\s*?->" with "h."
        - special cases:
            - HighsInfo/HighsOptions: somehow binding the single properties of HighsInfoStruct / HighsOptionsStruct does not work because something is not const
            (https://github.com/emscripten-core/emscripten/issues/14871). Instead comment this and bind to "records" property. + define InfoRecord/OptionsRecord
*/

EMSCRIPTEN_DECLARE_VAL_TYPE(DoubleArray)
EMSCRIPTEN_DECLARE_VAL_TYPE(IntArray)
EMSCRIPTEN_DECLARE_VAL_TYPE(HighsVarTypeArray)

template <typename T, typename... Args>
T make_JsArray(Args&&... args)
{
    static_assert(std::is_base_of<emscripten::val, T>::value, "T must derive from emscripten::val");
    T res = T(emscripten::val::array());
    res.template call<void>("push", std::forward<Args>(args)...);
    return res;
}

EMSCRIPTEN_DECLARE_VAL_TYPE(OptionType)
HighsStatus highs_setOptionValue(Highs& self, string name, OptionType value){
    if(value.isString()){
        return self.setOptionValue(name, value.as<string>());
    } else if(value.isTrue() || value.isFalse()) {
        return self.setOptionValue(name, value.as<bool>());
    } else if(value.isNumber()){
        HighsOptionType retval;
        auto result = self.getOptionType(name, retval);
        if(result != HighsStatus::kOk){return result;}
        if(retval == HighsOptionType::kDouble){
            return self.setOptionValue(name, value.as<double>());
        } else if(retval == HighsOptionType::kInt){
            return self.setOptionValue(name, value.as<HighsInt>());
        } else return HighsStatus::kError;
    } else return HighsStatus::kError;
}

#pragma region Preamble here

// arrays are assumed to be contiguous c-style arrays of correct type
// * c_style forces the array to be stored in C-style contiguous order
// * forcecast converts the array to the correct type if needed
// template <typename T>
// using dense_array_t = py::array_t<T, py::array::c_style | py::array::forcecast>;

// // wrapper for raw pointers
// template <class T>
// class readonly_ptr_wrapper {
//  public:
//   readonly_ptr_wrapper() : ptr(nullptr) {}
//   readonly_ptr_wrapper(T* ptr) : ptr(ptr) {}
//   readonly_ptr_wrapper(const readonly_ptr_wrapper& other) : ptr(other.ptr) {}
//   T& operator*() const { return *ptr; }
//   T* operator->() const { return ptr; }
//   T* get() const { return ptr; }
//   T& operator[](std::size_t idx) const { return ptr[idx]; }
//   bool is_valid() { return ptr != nullptr; }

//   py::array_t<T, py::array::c_style> to_array(std::size_t size) {
//     return py::array_t<T, py::array::c_style>(py::buffer_info(
//         ptr, sizeof(T), py::format_descriptor<T>::format(), 1, {size}, {1}));
//   }

//  private:
//   T* ptr;
// };

HighsStatus highs_passModel(Highs& h, HighsModel &model)
{
    return h.passModel(model);
}

HighsStatus highs_passModelPointers(
    Highs& h, const HighsInt num_col, const HighsInt num_row,
    const HighsInt num_nz, const HighsInt q_num_nz, const HighsInt a_format,
    const HighsInt q_format, const HighsInt sense, const double offset,
    const DoubleArray col_cost, const DoubleArray col_lower,
    const DoubleArray col_upper,
    const DoubleArray row_lower,
    const DoubleArray row_upper,
    const IntArray a_start,
    const IntArray a_index, const DoubleArray a_value,
    const IntArray q_start,
    const IntArray q_index, const DoubleArray q_value,
    const IntArray integrality)
{
    auto col_cost_info = emscripten::vecFromJSArray<double>(col_cost);
    auto col_lower_info = emscripten::vecFromJSArray<double>(col_lower);
    auto col_upper_info = emscripten::vecFromJSArray<double>(col_upper);
    auto row_lower_info = emscripten::vecFromJSArray<double>(row_lower);
    auto row_upper_info = emscripten::vecFromJSArray<double>(row_upper);
    auto a_start_info = emscripten::vecFromJSArray<HighsInt>(a_start);
    auto a_index_info = emscripten::vecFromJSArray<HighsInt>(a_index);
    auto a_value_info = emscripten::vecFromJSArray<double>(a_value);
    auto q_start_info = emscripten::vecFromJSArray<HighsInt>(q_start);
    auto q_index_info = emscripten::vecFromJSArray<HighsInt>(q_index);
    auto q_value_info = emscripten::vecFromJSArray<double>(q_value);
    auto integrality_info = emscripten::vecFromJSArray<HighsInt>(integrality);

    const double *col_cost_ptr = static_cast<double *>(col_cost_info.data());
    const double *col_lower_ptr = static_cast<double *>(col_lower_info.data());
    const double *col_upper_ptr = static_cast<double *>(col_upper_info.data());
    const double *row_lower_ptr = static_cast<double *>(row_lower_info.data());
    const double *row_upper_ptr = static_cast<double *>(row_upper_info.data());
    const double *a_value_ptr = static_cast<double *>(a_value_info.data());
    const double *q_value_ptr = static_cast<double *>(q_value_info.data());
    const HighsInt *a_start_ptr = static_cast<HighsInt *>(a_start_info.data());
    const HighsInt *a_index_ptr = static_cast<HighsInt *>(a_index_info.data());
    const HighsInt *q_start_ptr = static_cast<HighsInt *>(q_start_info.data());
    const HighsInt *q_index_ptr = static_cast<HighsInt *>(q_index_info.data());
    const HighsInt *integrality_ptr =
        static_cast<HighsInt *>(integrality_info.data());

    return h.passModel(
        static_cast<HighsInt>(num_col), static_cast<HighsInt>(num_row),
        static_cast<HighsInt>(num_nz), static_cast<HighsInt>(q_num_nz),
        static_cast<HighsInt>(a_format), static_cast<HighsInt>(q_format),
        static_cast<HighsInt>(sense), offset, col_cost_ptr, col_lower_ptr,
        col_upper_ptr, row_lower_ptr, row_upper_ptr, a_start_ptr, a_index_ptr,
        a_value_ptr, q_start_ptr, q_index_ptr, q_value_ptr, integrality_ptr);
}

HighsStatus highs_passLp(Highs& h, HighsLp &lp) { return h.passModel(lp); }

HighsStatus highs_passLpPointers(Highs& h, const HighsInt num_col,
                                 const HighsInt num_row, const HighsInt num_nz,
                                 const HighsInt a_format, const HighsInt sense,
                                 const double offset,
                                 const DoubleArray col_cost,
                                 const DoubleArray col_lower,
                                 const DoubleArray col_upper,
                                 const DoubleArray row_lower,
                                 const DoubleArray row_upper,
                                 const IntArray a_start,
                                 const IntArray a_index,
                                 const DoubleArray a_value,
                                 const IntArray integrality)
{
    auto col_cost_info = emscripten::vecFromJSArray<double>(col_cost);
    auto col_lower_info = emscripten::vecFromJSArray<double>(col_lower);
    auto col_upper_info = emscripten::vecFromJSArray<double>(col_upper);
    auto row_lower_info = emscripten::vecFromJSArray<double>(row_lower);
    auto row_upper_info = emscripten::vecFromJSArray<double>(row_upper);
    auto a_start_info = emscripten::vecFromJSArray<HighsInt>(a_start);
    auto a_index_info = emscripten::vecFromJSArray<HighsInt>(a_index);
    auto a_value_info = emscripten::vecFromJSArray<double>(a_value);
    auto integrality_info = emscripten::vecFromJSArray<HighsInt>(integrality);

    const double *col_cost_ptr = static_cast<double *>(col_cost_info.data());
    const double *col_lower_ptr = static_cast<double *>(col_lower_info.data());
    const double *col_upper_ptr = static_cast<double *>(col_upper_info.data());
    const double *row_lower_ptr = static_cast<double *>(row_lower_info.data());
    const double *row_upper_ptr = static_cast<double *>(row_upper_info.data());
    const HighsInt *a_start_ptr = static_cast<HighsInt *>(a_start_info.data());
    const HighsInt *a_index_ptr = static_cast<HighsInt *>(a_index_info.data());
    const double *a_value_ptr = static_cast<double *>(a_value_info.data());
    const HighsInt *integrality_ptr =
        static_cast<HighsInt *>(integrality_info.data());

    return h.passModel(
        static_cast<HighsInt>(num_col), static_cast<HighsInt>(num_row),
        static_cast<HighsInt>(num_nz), static_cast<HighsInt>(a_format),
        static_cast<HighsInt>(sense), offset, col_cost_ptr, col_lower_ptr,
        col_upper_ptr, row_lower_ptr, row_upper_ptr, a_start_ptr, a_index_ptr,
        a_value_ptr, integrality_ptr);
}

HighsStatus highs_passHessian(Highs& h, HighsHessian &hessian)
{
    return h.passHessian(hessian);
}

HighsStatus highs_passHessianPointers(Highs& h, const HighsInt dim,
                                      const HighsInt num_nz,
                                      const HighsInt format,
                                      const IntArray q_start,
                                      const IntArray q_index,
                                      const DoubleArray q_value)
{
    auto q_start_info = emscripten::vecFromJSArray<HighsInt>(q_start);
    auto q_index_info = emscripten::vecFromJSArray<HighsInt>(q_index);
    auto q_value_info = emscripten::vecFromJSArray<double>(q_value);

    const HighsInt *q_start_ptr = static_cast<HighsInt *>(q_start_info.data());
    const HighsInt *q_index_ptr = static_cast<HighsInt *>(q_index_info.data());
    const double *q_value_ptr = static_cast<double *>(q_value_info.data());

    return h.passHessian(dim, num_nz, format, q_start_ptr, q_index_ptr,
                          q_value_ptr);
}

HighsStatus highs_postsolve(Highs& h, const HighsSolution &solution,
                            const HighsBasis &basis)
{
    return h.postsolve(solution, basis);
}

HighsStatus highs_mipPostsolve(Highs& h, const HighsSolution &solution)
{
    return h.postsolve(solution);
}

HighsStatus highs_writeSolution(Highs& h, const std::string filename,
                                const SolutionStyle style)
{
    return h.writeSolution(filename, style);
}

// Not needed once getModelStatus(const bool scaled_model) disappears
// from, Highs.h
HighsModelStatus highs_getModelStatus(Highs& h) { return h.getModelStatus(); }

// TODO: test if it works like intended. if yes transliterate all tuple-returning functions
EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getRangingResult)
highs_getRangingResult highs_getRanging(Highs& h)
{
    HighsRanging ranging;
    HighsStatus status = h.getRanging(ranging);
    return make_JsArray<highs_getRangingResult>(status, ranging);
}

HighsStatus highs_addRow(Highs& h, double lower, double upper,
                         HighsInt num_new_nz, IntArray indices,
                         DoubleArray values)
{
    auto indices_info = emscripten::vecFromJSArray<HighsInt>(indices);
    auto values_info = emscripten::vecFromJSArray<double>(values);

    HighsInt *indices_ptr = reinterpret_cast<HighsInt *>(indices_info.data());
    double *values_ptr = static_cast<double *>(values_info.data());

    return h.addRow(lower, upper, num_new_nz, indices_ptr, values_ptr);
}

HighsStatus highs_addRows(Highs& h, HighsInt num_row,
                          DoubleArray lower,
                          DoubleArray upper, HighsInt num_new_nz,
                          IntArray starts,
                          IntArray indices,
                          DoubleArray values)
{
    auto lower_info = emscripten::vecFromJSArray<double>(lower);
    auto upper_info = emscripten::vecFromJSArray<double>(upper);
    auto starts_info = emscripten::vecFromJSArray<HighsInt>(starts);
    auto indices_info = emscripten::vecFromJSArray<HighsInt>(indices);
    auto values_info = emscripten::vecFromJSArray<double>(values);

    double *lower_ptr = static_cast<double *>(lower_info.data());
    double *upper_ptr = static_cast<double *>(upper_info.data());
    HighsInt *starts_ptr = reinterpret_cast<HighsInt *>(starts_info.data());
    HighsInt *indices_ptr = reinterpret_cast<HighsInt *>(indices_info.data());
    double *values_ptr = static_cast<double *>(values_info.data());

    return h.addRows(num_row, lower_ptr, upper_ptr, num_new_nz, starts_ptr,
                      indices_ptr, values_ptr);
}

HighsStatus highs_addCol(Highs& h, double cost, double lower, double upper,
                         HighsInt num_new_nz, IntArray indices,
                         DoubleArray values)
{
    auto indices_info = emscripten::vecFromJSArray<HighsInt>(indices);
    auto values_info = emscripten::vecFromJSArray<double>(values);

    HighsInt *indices_ptr = reinterpret_cast<HighsInt *>(indices_info.data());
    double *values_ptr = static_cast<double *>(values_info.data());

    return h.addCol(cost, lower, upper, num_new_nz, indices_ptr, values_ptr);
}

HighsStatus highs_addCols(Highs& h, HighsInt num_col,
                          DoubleArray cost,
                          DoubleArray lower,
                          DoubleArray upper, HighsInt num_new_nz,
                          IntArray starts,
                          IntArray indices,
                          DoubleArray values)
{
    auto cost_info = emscripten::vecFromJSArray<double>(cost);
    auto lower_info = emscripten::vecFromJSArray<double>(lower);
    auto upper_info = emscripten::vecFromJSArray<double>(upper);
    auto starts_info = emscripten::vecFromJSArray<HighsInt>(starts);
    auto indices_info = emscripten::vecFromJSArray<HighsInt>(indices);
    auto values_info = emscripten::vecFromJSArray<double>(values);

    double *cost_ptr = static_cast<double *>(cost_info.data());
    double *lower_ptr = static_cast<double *>(lower_info.data());
    double *upper_ptr = static_cast<double *>(upper_info.data());
    HighsInt *starts_ptr = reinterpret_cast<HighsInt *>(starts_info.data());
    const HighsInt *indices_ptr = reinterpret_cast<HighsInt *>(indices_info.data());
    double *values_ptr = static_cast<double *>(values_info.data());

    return h.addCols(num_col, cost_ptr, lower_ptr, upper_ptr, num_new_nz,
                      starts_ptr, indices_ptr, values_ptr);
}

HighsStatus highs_addVar(Highs& h, double lower, double upper)
{
    return h.addVar(lower, upper);
}

HighsStatus highs_addVars(Highs& h, HighsInt num_vars,
                          DoubleArray lower,
                          DoubleArray upper)
{
    auto lower_info = emscripten::vecFromJSArray<double>(lower);
    auto upper_info = emscripten::vecFromJSArray<double>(upper);

    double *lower_ptr = static_cast<double *>(lower_info.data());
    double *upper_ptr = static_cast<double *>(upper_info.data());

    return h.addVars(num_vars, lower_ptr, upper_ptr);
}

HighsStatus highs_changeColsCost(Highs& h, HighsInt num_set_entries,
                                 IntArray indices,
                                 DoubleArray cost)
{
    auto indices_info = emscripten::vecFromJSArray<HighsInt>(indices);
    auto cost_info = emscripten::vecFromJSArray<double>(cost);

    HighsInt *indices_ptr = static_cast<HighsInt *>(indices_info.data());
    double *cost_ptr = static_cast<double *>(cost_info.data());

    return h.changeColsCost(num_set_entries, indices_ptr, cost_ptr);
}

HighsStatus highs_changeColsBounds(Highs& h, HighsInt num_set_entries,
                                   IntArray indices,
                                   DoubleArray lower,
                                   DoubleArray upper)
{
    auto indices_info = emscripten::vecFromJSArray<HighsInt>(indices);
    auto lower_info = emscripten::vecFromJSArray<double>(lower);
    auto upper_info = emscripten::vecFromJSArray<double>(upper);

    HighsInt *indices_ptr = static_cast<HighsInt *>(indices_info.data());
    double *lower_ptr = static_cast<double *>(lower_info.data());
    double *upper_ptr = static_cast<double *>(upper_info.data());

    return h.changeColsBounds(num_set_entries, indices_ptr, lower_ptr,
                               upper_ptr);
}

HighsStatus highs_changeColsIntegrality(
    Highs& h, HighsInt num_set_entries, IntArray indices,
    HighsVarTypeArray integrality)
{
    auto indices_info = emscripten::vecFromJSArray<HighsInt>(indices);
    auto integrality_info = emscripten::vecFromJSArray<HighsVarType>(integrality);

    HighsInt *indices_ptr = static_cast<HighsInt *>(indices_info.data());
    HighsVarType *integrality_ptr =
        static_cast<HighsVarType *>(integrality_info.data());

    return h.changeColsIntegrality(num_set_entries, indices_ptr,
                                    integrality_ptr);
}

// Same as deleteVars
HighsStatus highs_deleteCols(Highs& h, HighsInt num_set_entries,
                             IntArray indices)
{
    auto index_info = emscripten::vecFromJSArray<HighsInt>(indices);
    HighsInt *index_ptr = reinterpret_cast<HighsInt *>(index_info.data());
    return h.deleteCols(num_set_entries, index_ptr);
}

HighsStatus highs_deleteRows(Highs& h, HighsInt num_set_entries,
                             IntArray indices)
{
    auto index_info = emscripten::vecFromJSArray<HighsInt>(indices);
    HighsInt *index_ptr = reinterpret_cast<HighsInt *>(index_info.data());
    return h.deleteRows(num_set_entries, index_ptr);
}

HighsStatus highs_setSolution(Highs& h, HighsSolution &solution)
{
    return h.setSolution(solution);
}

HighsStatus highs_setSparseSolution(Highs& h, HighsInt num_entries,
                                    IntArray index,
                                    DoubleArray value)
{
    auto index_info = emscripten::vecFromJSArray<HighsInt>(index);
    auto value_info = emscripten::vecFromJSArray<double>(value);

    HighsInt *index_ptr = reinterpret_cast<HighsInt *>(index_info.data());
    double *value_ptr = static_cast<double *>(value_info.data());

    return h.setSolution(num_entries, index_ptr, value_ptr);
}

HighsStatus highs_setBasis(Highs& h, HighsBasis &basis)
{
    return h.setBasis(basis);
}

HighsStatus highs_setLogicalBasis(Highs& h) { return h.setBasis(); }

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getOptionValueResult)
highs_getOptionValueResult highs_getOptionValue( // types: HighsStatus, py::object
    Highs& h, const std::string& option) {
  HighsOptionType option_type;
  HighsStatus status = h.getOptionType(option, option_type);

  if (status != HighsStatus::kOk) return make_JsArray<highs_getOptionValueResult>(status, emscripten::val::undefined());

  if (option_type == HighsOptionType::kBool) {
    bool value;
    status = h.getOptionValue(option, value);
    return make_JsArray<highs_getOptionValueResult>(status, value);
  } else if (option_type == HighsOptionType::kInt) {
    HighsInt value;
    status = h.getOptionValue(option, value);
    return make_JsArray<highs_getOptionValueResult>(status, value);
  } else if (option_type == HighsOptionType::kDouble) {
    double value;
    status = h.getOptionValue(option, value);
    return make_JsArray<highs_getOptionValueResult>(status, value);
  } else if (option_type == HighsOptionType::kString) {
    std::string value;
    status = h.getOptionValue(option, value);
    return make_JsArray<highs_getOptionValueResult>(status, value);
  } else
    return make_JsArray<highs_getOptionValueResult>(HighsStatus::kError, emscripten::val::undefined());
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getOptionTypeResult)
highs_getOptionTypeResult highs_getOptionType(
    Highs& h, const std::string &option)
{
    HighsOptionType option_type;
    HighsStatus status = h.getOptionType(option, option_type);
    return make_JsArray<highs_getOptionTypeResult>(status, option_type);
}

HighsStatus highs_writeOptions(Highs& h, const std::string &filename)
{
    return h.writeOptions(filename);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getInfoValueResult)
highs_getInfoValueResult highs_getInfoValue( // types: HighsStatus, py::object
    Highs& h, const std::string& info) {
  HighsInfoType info_type;
  HighsStatus status = h.getInfoType(info, info_type);

  if (status != HighsStatus::kOk) return make_JsArray<highs_getInfoValueResult>(status, emscripten::val::undefined());

  if (info_type == HighsInfoType::kInt64) {
    int64_t value;
    status = h.getInfoValue(info, value);
    return make_JsArray<highs_getInfoValueResult>(status, value);
  } else if (info_type == HighsInfoType::kInt) {
    HighsInt value;
    status = h.getInfoValue(info, value);
    return make_JsArray<highs_getInfoValueResult>(status, value);
  } else if (info_type == HighsInfoType::kDouble) {
    double value;
    status = h.getInfoValue(info, value);
    return make_JsArray<highs_getInfoValueResult>(status, value);
  } else
    return make_JsArray<highs_getInfoValueResult>(HighsStatus::kError, emscripten::val::undefined());
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getInfoTypeResult)
highs_getInfoTypeResult highs_getInfoType( // types: HighsStatus, HighsInfoType
    Highs& h, const std::string &info)
{
    HighsInfoType info_type;
    HighsStatus status = h.getInfoType(info, info_type);
    return make_JsArray<highs_getInfoTypeResult>(status, info_type);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getObjectiveSenseResult)
highs_getObjectiveSenseResult highs_getObjectiveSense(Highs& h)
{ // types: HighsStatus, ObjSense
    ObjSense obj_sense;
    HighsStatus status = h.getObjectiveSense(obj_sense);
    return make_JsArray<highs_getObjectiveSenseResult>(status, obj_sense);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getObjectiveOffsetResult)
highs_getObjectiveOffsetResult highs_getObjectiveOffset(Highs& h)
{ // types: HighsStatus, double
    double obj_offset;
    HighsStatus status = h.getObjectiveOffset(obj_offset);
    return make_JsArray<highs_getObjectiveOffsetResult>(status, obj_offset);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getColResult)
highs_getColResult highs_getCol( // types: HighsStatus, double, double, double, HighsInt
    Highs& h, HighsInt col)
{
    double cost, lower, upper;
    HighsInt get_num_col;
    HighsInt get_num_nz;
    HighsInt col_ = static_cast<HighsInt>(col);
    HighsStatus status = h.getCols(1, &col_, get_num_col, &cost, &lower, &upper,
                                    get_num_nz, nullptr, nullptr, nullptr);
    return make_JsArray<highs_getColResult>(status, cost, lower, upper, get_num_nz);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getColEntriesResult)
highs_getColEntriesResult highs_getColEntries(Highs& h, HighsInt col) { // types: HighsStatus, IntArray, DoubleArray
  double cost, lower, upper;
  HighsInt get_num_col;
  HighsInt get_num_nz;
  HighsInt col_ = static_cast<HighsInt>(col);
  h.getCols(1, &col_, get_num_col, nullptr, nullptr, nullptr, get_num_nz,
             nullptr, nullptr, nullptr);
  get_num_nz = get_num_nz > 0 ? get_num_nz : 1;
  HighsInt start;
  std::vector<HighsInt> index(get_num_nz);
  std::vector<double> value(get_num_nz);
  HighsInt* index_ptr = static_cast<HighsInt*>(index.data());
  double* value_ptr = static_cast<double*>(value.data());
  HighsStatus status =
      h.getCols(1, &col_, get_num_col, nullptr, nullptr, nullptr, get_num_nz,
                 &start, index_ptr, value_ptr);
  return make_JsArray<highs_getColEntriesResult>(status, emscripten::val::array(index), emscripten::val::array(value));
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getRowResult)
highs_getRowResult highs_getRow(Highs& h, // types: HighsStatus, double, double, HighsInt
                                HighsInt row)
{
    double cost, lower, upper;
    HighsInt get_num_row;
    HighsInt get_num_nz;
    HighsInt row_ = static_cast<HighsInt>(row);
    HighsStatus status = h.getRows(1, &row_, get_num_row, &lower, &upper,
                                    get_num_nz, nullptr, nullptr, nullptr);
    return make_JsArray<highs_getRowResult>(status, lower, upper, get_num_nz);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getRowEntriesResult)
highs_getRowEntriesResult highs_getRowEntries(Highs& h, HighsInt row) { // types: HighsStatus, IntArray, DoubleArray
  double cost, lower, upper;
  HighsInt get_num_row;
  HighsInt get_num_nz;
  HighsInt row_ = static_cast<HighsInt>(row);
  h.getRows(1, &row_, get_num_row, nullptr, nullptr, get_num_nz, nullptr,
             nullptr, nullptr);
  get_num_nz = get_num_nz > 0 ? get_num_nz : 1;
  HighsInt start;
  std::vector<HighsInt> index(get_num_nz);
  std::vector<double> value(get_num_nz);
  HighsInt* index_ptr = static_cast<HighsInt*>(index.data());
  double* value_ptr = static_cast<double*>(value.data());
  HighsStatus status = h.getRows(1, &row_, get_num_row, nullptr, nullptr,
                                  get_num_nz, &start, index_ptr, value_ptr);
  return make_JsArray<highs_getRowEntriesResult>(status, emscripten::val::array(index), emscripten::val::array(value));
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getColsResult)
highs_getColsResult highs_getCols(Highs& h, HighsInt num_set_entries,
              IntArray indices) {
  auto indices_info  = emscripten::vecFromJSArray<HighsInt>(indices);
  HighsInt* indices_ptr = static_cast<HighsInt*>(indices_info.data());
  // Make sure that the vectors are not empty
  const HighsInt dim = num_set_entries > 0 ? num_set_entries : 1;
  std::vector<double> cost(dim);
  std::vector<double> lower(dim);
  std::vector<double> upper(dim);
  double* cost_ptr = static_cast<double*>(cost.data());
  double* lower_ptr = static_cast<double*>(lower.data());
  double* upper_ptr = static_cast<double*>(upper.data());
  HighsInt get_num_col;
  HighsInt get_num_nz;
  HighsStatus status =
      h.getCols(num_set_entries, indices_ptr, get_num_col, cost_ptr, lower_ptr,
                 upper_ptr, get_num_nz, nullptr, nullptr, nullptr);
  return make_JsArray<highs_getColsResult>(status, get_num_col, emscripten::val::array(cost), emscripten::val::array(lower),
                         emscripten::val::array(upper), get_num_nz);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getColsEntriesResult)
highs_getColsEntriesResult highs_getColsEntries(Highs& h, HighsInt num_set_entries,
                     IntArray indices) {
  auto indices_info  = emscripten::vecFromJSArray<HighsInt>(indices);
  HighsInt* indices_ptr = static_cast<HighsInt*>(indices_info.data());
  // Make sure that the vectors are not empty
  const HighsInt dim = num_set_entries > 0 ? num_set_entries : 1;
  HighsInt get_num_col;
  HighsInt get_num_nz;
  h.getCols(num_set_entries, indices_ptr, get_num_col, nullptr, nullptr,
             nullptr, get_num_nz, nullptr, nullptr, nullptr);
  get_num_nz = get_num_nz > 0 ? get_num_nz : 1;
  std::vector<HighsInt> start(dim);
  std::vector<HighsInt> index(get_num_nz);
  std::vector<double> value(get_num_nz);
  HighsInt* start_ptr = static_cast<HighsInt*>(start.data());
  HighsInt* index_ptr = static_cast<HighsInt*>(index.data());
  double* value_ptr = static_cast<double*>(value.data());
  HighsStatus status =
      h.getCols(num_set_entries, indices_ptr, get_num_col, nullptr, nullptr,
                 nullptr, get_num_nz, start_ptr, index_ptr, value_ptr);
  return make_JsArray<highs_getColsEntriesResult>(status, emscripten::val::array(start), emscripten::val::array(index),
                         emscripten::val::array(value));
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getRowsResult)
highs_getRowsResult highs_getRows(Highs& h, HighsInt num_set_entries,
              IntArray indices) {
  auto indices_info  = emscripten::vecFromJSArray<HighsInt>(indices);
  HighsInt* indices_ptr = static_cast<HighsInt*>(indices_info.data());
  // Make sure that the vectors are not empty
  const HighsInt dim = num_set_entries > 0 ? num_set_entries : 1;
  std::vector<double> lower(dim);
  std::vector<double> upper(dim);
  double* lower_ptr = static_cast<double*>(lower.data());
  double* upper_ptr = static_cast<double*>(upper.data());
  HighsInt get_num_row;
  HighsInt get_num_nz;
  HighsStatus status =
      h.getRows(num_set_entries, indices_ptr, get_num_row, lower_ptr,
                 upper_ptr, get_num_nz, nullptr, nullptr, nullptr);
  return make_JsArray<highs_getRowsResult>(status, get_num_row, emscripten::val::array(lower), emscripten::val::array(upper),
                         get_num_nz);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getRowsEntriesResult)
highs_getRowsEntriesResult highs_getRowsEntries(Highs& h, HighsInt num_set_entries,
                     IntArray indices) {
  auto indices_info  = emscripten::vecFromJSArray<HighsInt>(indices);
  HighsInt* indices_ptr = static_cast<HighsInt*>(indices_info.data());
  // Make sure that the vectors are not empty
  const HighsInt dim = num_set_entries > 0 ? num_set_entries : 1;
  HighsInt get_num_row;
  HighsInt get_num_nz;
  h.getRows(num_set_entries, indices_ptr, get_num_row, nullptr, nullptr,
             get_num_nz, nullptr, nullptr, nullptr);
  get_num_nz = get_num_nz > 0 ? get_num_nz : 1;
  std::vector<HighsInt> start(dim);
  std::vector<HighsInt> index(get_num_nz);
  std::vector<double> value(get_num_nz);
  HighsInt* start_ptr = static_cast<HighsInt*>(start.data());
  HighsInt* index_ptr = static_cast<HighsInt*>(index.data());
  double* value_ptr = static_cast<double*>(value.data());
  HighsStatus status =
      h.getRows(num_set_entries, indices_ptr, get_num_row, nullptr, nullptr,
                 get_num_nz, start_ptr, index_ptr, value_ptr);
  return make_JsArray<highs_getRowsEntriesResult>(status, emscripten::val::array(start), emscripten::val::array(index),
                         emscripten::val::array(value));
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getColNameResult)
highs_getColNameResult highs_getColName(Highs& h, // types: HighsStatus, std::string
                                        const HighsInt col)
{
    std::string name;
    HighsStatus status = h.getColName(col, name);
    return make_JsArray<highs_getColNameResult>(status, name);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getColByNameResult)
highs_getColByNameResult highs_getColByName(Highs& h, // types: HighsStatus, int
                                            const std::string name)
{
    HighsInt col;
    HighsStatus status = h.getColByName(name, col);
    return make_JsArray<highs_getColByNameResult>(status, col);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getRowNameResult)
highs_getRowNameResult highs_getRowName(Highs& h, // types: HighsStatus, std::string
                                        const HighsInt row)
{
    std::string name;
    HighsStatus status = h.getRowName(row, name);
    return make_JsArray<highs_getRowNameResult>(status, name);
}

EMSCRIPTEN_DECLARE_VAL_TYPE(highs_getRowByNameResult)
highs_getRowByNameResult highs_getRowByName(Highs& h, // types: HighsStatus, int
                                            const std::string name)
{
    HighsInt row;
    HighsStatus status = h.getRowByName(name, row);
    return make_JsArray<highs_getRowByNameResult>(status, row);
}

// Wrap the setCallback function to appropriately handle user data.
// pybind11 automatically ensures GIL is re-acquired when the callback is called.
EMSCRIPTEN_DECLARE_VAL_TYPE(Callback)
EMSCRIPTEN_DECLARE_VAL_TYPE(CallbackData)
HighsStatus highs_setCallback(
    Highs& h,
    Callback /*std::function<void(int, const std::string&, const HighsCallbackDataOut*, HighsCallbackDataIn*, py::handle)>*/ fn,
    CallbackData user_callback_data) {
  if (!fn.hasOwnProperty("call")) // TODO: correct way to determine if it's a function?
    return h.setCallback((HighsCallbackFunctionType) nullptr, nullptr);
  else
    return h.setCallback(
        [fn, user_callback_data](int callbackType, const std::string& msg,
                   const HighsCallbackDataOut* dataOut,
                   HighsCallbackDataIn* dataIn, void* d) {
          return fn(callbackType, msg, dataOut, dataIn, emscripten::val(d));
        },
        user_callback_data.as_handle());
}

#pragma endregion

HighsStatus highs_feasibilityRelaxation(Highs& self, double global_lower_penalty,
             double global_upper_penalty, double global_rhs_penalty,
             DoubleArray local_lower_penalty, DoubleArray local_upper_penalty,
             DoubleArray local_rhs_penalty) {
            std::vector<double> llp, lup, lrp;
            const double* llp_ptr = nullptr;
            const double* lup_ptr = nullptr;
            const double* lrp_ptr = nullptr;

            if (local_lower_penalty.isArray()) {
              llp = emscripten::vecFromJSArray<double>(local_lower_penalty);
              llp_ptr = llp.data();
            }
            if (local_upper_penalty.isArray()) {
              lup = emscripten::vecFromJSArray<double>(local_upper_penalty);
              lup_ptr = lup.data();
            }
            if (local_rhs_penalty.isArray()) {
              lrp = emscripten::vecFromJSArray<double>(local_rhs_penalty);
              lrp_ptr = lrp.data();
            }

            return self.feasibilityRelaxation(
                global_lower_penalty, global_upper_penalty, global_rhs_penalty,
                llp_ptr, lup_ptr, lrp_ptr);
          }

EMSCRIPTEN_BINDINGS(Highs)
{
#pragma region vector definitions
    emscripten::register_vector<HighsInt>("IntVector");
    emscripten::register_vector<double>("DoubleVector");
    emscripten::register_vector<string>("StringVector");
    emscripten::register_vector<HighsVarType>("HighsVarTypeVector");
    emscripten::register_vector<HighsObjectiveSolution>("HighsObjectiveSolutionVector");
    emscripten::register_vector<HighsIisInfo>("HighsIisInfoVector");
    emscripten::register_vector<HighsBasisStatus>("HighsBasisStatusVector");
    emscripten::register_vector<InfoRecord *>("InfoRecordVector");
    emscripten::register_vector<OptionRecord *>("OptionRecordVector");
#pragma endregion

#pragma region optional definitions
    emscripten::register_optional<InfoRecord>();
    emscripten::register_optional<OptionRecord>();
#pragma endregion

#pragma region type definitions
    emscripten::register_type<DoubleArray>("Array<number>");
    // TODO: how to propagate that int is expected here?
    emscripten::register_type<IntArray>("Array<number>");
    emscripten::register_type<HighsVarTypeArray>("Array<HighsVarType>");
    emscripten::register_type<CallbackData>("any");
    emscripten::register_type<Callback>("(callbackType: number, msg: string, dataOut: HighsCallbackDataOut, dataIn: HighsCallbackDataIn, user_callback_data: any) => void");
    emscripten::register_type<OptionType>("string | boolean | number");
    // ##TYPES##
    emscripten::register_type<highs_getRangingResult>("[status: HighsStatus, ranging: HighsRanging]");
    emscripten::register_type<highs_getOptionValueResult>("[status: HighsStatus, value: bool|string|number|undefined]");
    emscripten::register_type<highs_getOptionTypeResult>("[status: HighsStatus, optionType: HighsOptionType]");
    emscripten::register_type<highs_getInfoValueResult>("[status: HighsStatus, value: number|undefined]");
    emscripten::register_type<highs_getInfoTypeResult>("[status: HighsStatus, infoType: HighsInfoType]");
    emscripten::register_type<highs_getObjectiveSenseResult>("[status: HighsStatus, objSense: ObjSense]");
    emscripten::register_type<highs_getObjectiveOffsetResult>("[status: HighsStatus, offset: number]");
    emscripten::register_type<highs_getColResult>("[status: HighsStatus, cost: number, lower: number, upper: number, get_num_nz: number]");
    emscripten::register_type<highs_getColEntriesResult>("[status: HighsStatus, index: number[], value: number[]]");
    emscripten::register_type<highs_getRowResult>("[status: HighsStatus, lower: number, upper: number, get_num_nz: number]");
    emscripten::register_type<highs_getRowEntriesResult>("[status: HighsStatus, index: number[], value: number[]]");
    emscripten::register_type<highs_getColsResult>("[status: HighsStatus, get_num_col: number, cost: number[], lower: number[], upper: number[], get_num_nz: number]");
    emscripten::register_type<highs_getColsEntriesResult>("[status: HighsStatus, start: number[], index: number[], value: number[]]");
    emscripten::register_type<highs_getRowsResult>("[status: HighsStatus, get_num_row: number, cost: number[], lower: number[], upper: number[], get_num_nz: number]");
    emscripten::register_type<highs_getRowsEntriesResult>("[status: HighsStatus, start: number[], index: number[], value: number[]]");
    emscripten::register_type<highs_getColNameResult>("[status: HighsStatus, name: string]");
    emscripten::register_type<highs_getColByNameResult>("[status: HighsStatus, col: number]");
    emscripten::register_type<highs_getRowNameResult>("[status: HighsStatus, name: string]");
    emscripten::register_type<highs_getRowByNameResult>("[status: HighsStatus, row: number]");
#pragma endregion

#pragma region manually added bindings
    emscripten::class_<InfoRecord>("InfoRecord")
        .constructor<HighsInfoType, string, string, bool>()
        .property("type", &InfoRecord::type)
        .property("name", &InfoRecord::name)
        .property("description", &InfoRecord::description)
        .property("advanced", &InfoRecord::advanced);
    // TODO: The concrete classes somehow lead to errors (unregistered type ...)
    // emscripten::class_<InfoRecordInt, emscripten::base<InfoRecord>>("InfoRecordInt")
    //     .constructor<string, string, bool, HighsInt *, HighsInt>()
    //     .property("value", &InfoRecordInt::value, emscripten::allow_raw_pointers())
    //     .property("default_value", &InfoRecordInt::default_value);
    // emscripten::class_<InfoRecordDouble, emscripten::base<InfoRecord>>("InfoRecordDouble")
    //     .constructor<string, string, bool, double *, double>()
    //     .property("value", &InfoRecordDouble::value, emscripten::allow_raw_pointers())
    //     .property("default_value", &InfoRecordDouble::default_value);
    // emscripten::class_<InfoRecordInt64, emscripten::base<InfoRecord>>("InfoRecordInt64")
    //     .constructor<string, string, bool, int64_t *, int64_t>()
    //     .property("value", &InfoRecordInt64::value, emscripten::allow_raw_pointers())
    //     .property("default_value", &InfoRecordInt64::default_value);

    emscripten::class_<OptionRecord>("OptionRecord")
        .constructor<HighsOptionType, string, string, bool>()
        .property("type", &OptionRecord::type)
        .property("name", &OptionRecord::name)
        .property("description", &OptionRecord::description)
        .property("advanced", &OptionRecord::advanced);
    // emscripten::class_<OptionRecordInt, emscripten::base<OptionRecord>>("OptionRecordInt")
    //     .constructor<string, string, bool, HighsInt *, HighsInt, HighsInt, HighsInt>()
    //     .property("value", &OptionRecordInt::value, emscripten::allow_raw_pointers())
    //     .property("default_value", &OptionRecordInt::default_value)
    //     .property("lower_bound", &OptionRecordInt::lower_bound)
    //     .property("upper_bound", &OptionRecordInt::upper_bound)
    //     .function("assignvalue", &OptionRecordInt::assignvalue);
    // emscripten::class_<OptionRecordDouble, emscripten::base<OptionRecord>>("OptionRecordDouble")
    //     .constructor<string, string, bool, double *, double, double, double>()
    //     .property("value", &OptionRecordDouble::value, emscripten::allow_raw_pointers())
    //     .property("default_value", &OptionRecordDouble::default_value)
    //     .property("lower_bound", &OptionRecordDouble::lower_bound)
    //     .property("upper_bound", &OptionRecordDouble::upper_bound)
    //     .function("assignvalue", &OptionRecordDouble::assignvalue);
    // emscripten::class_<OptionRecordBool, emscripten::base<OptionRecord>>("OptionRecordBool")
    //     .constructor<string, string, bool, bool *, bool>()
    //     .property("value", &OptionRecordBool::value, emscripten::allow_raw_pointers())
    //     .property("default_value", &OptionRecordBool::default_value)
    //     .function("assignvalue", &OptionRecordBool::assignvalue);
    // emscripten::class_<OptionRecordString, emscripten::base<OptionRecord>>("OptionRecordString")
    //     .constructor<string, string, bool, string *, string>()
    //     .property("value", &OptionRecordString::value, emscripten::allow_raw_pointers())
    //     .property("default_value", &OptionRecordString::default_value)
    //     .function("assignvalue", &OptionRecordString::assignvalue);
    emscripten::enum_<SolutionStyle>("SolutionStyle")
        .value("kSolutionStyleOldRaw",SolutionStyle::kSolutionStyleOldRaw)
        .value("kSolutionStyleRaw",SolutionStyle::kSolutionStyleRaw)
        .value("kSolutionStylePretty",SolutionStyle::kSolutionStylePretty)
        .value("kSolutionStyleGlpsolRaw",SolutionStyle::kSolutionStyleGlpsolRaw)
        .value("kSolutionStyleGlpsolPretty",SolutionStyle::kSolutionStyleGlpsolPretty)
        .value("kSolutionStyleSparse",SolutionStyle::kSolutionStyleSparse);
#pragma endregion

#pragma region PYBIND11_MODULE content here

    // enum classes
    py::enum_<ObjSense>("ObjSense")
        .value("kMinimize", ObjSense::kMinimize)
        .value("kMaximize", ObjSense::kMaximize);
    // // ;
    py::enum_<MatrixFormat>("MatrixFormat")
        .value("kColwise", MatrixFormat::kColwise)
        .value("kRowwise", MatrixFormat::kRowwise)
        .value("kRowwisePartitioned", MatrixFormat::kRowwisePartitioned);
    // // ;
    py::enum_<HessianFormat>("HessianFormat")
        .value("kTriangular", HessianFormat::kTriangular)
        .value("kSquare", HessianFormat::kSquare);
    // ;
    py::enum_<SolutionStatus>("SolutionStatus")
        .value("kSolutionStatusNone", SolutionStatus::kSolutionStatusNone)
        .value("kSolutionStatusInfeasible",
               SolutionStatus::kSolutionStatusInfeasible)
        .value("kSolutionStatusFeasible", SolutionStatus::kSolutionStatusFeasible);
    py::enum_<BasisValidity>("BasisValidity")
        .value("kBasisValidityInvalid", BasisValidity::kBasisValidityInvalid)
        .value("kBasisValidityValid", BasisValidity::kBasisValidityValid);
    py::enum_<HighsModelStatus>("HighsModelStatus")
        .value("kNotset", HighsModelStatus::kNotset)
        .value("kLoadError", HighsModelStatus::kLoadError)
        .value("kModelError", HighsModelStatus::kModelError)
        .value("kPresolveError", HighsModelStatus::kPresolveError)
        .value("kSolveError", HighsModelStatus::kSolveError)
        .value("kPostsolveError", HighsModelStatus::kPostsolveError)
        .value("kModelEmpty", HighsModelStatus::kModelEmpty)
        .value("kOptimal", HighsModelStatus::kOptimal)
        .value("kInfeasible", HighsModelStatus::kInfeasible)
        .value("kUnboundedOrInfeasible", HighsModelStatus::kUnboundedOrInfeasible)
        .value("kUnbounded", HighsModelStatus::kUnbounded)
        .value("kObjectiveBound", HighsModelStatus::kObjectiveBound)
        .value("kObjectiveTarget", HighsModelStatus::kObjectiveTarget)
        .value("kTimeLimit", HighsModelStatus::kTimeLimit)
        .value("kIterationLimit", HighsModelStatus::kIterationLimit)
        .value("kUnknown", HighsModelStatus::kUnknown)
        .value("kSolutionLimit", HighsModelStatus::kSolutionLimit)
        .value("kInterrupt", HighsModelStatus::kInterrupt)
        .value("kMemoryLimit", HighsModelStatus::kMemoryLimit);
    // ;
    py::enum_<HighsPresolveStatus>("HighsPresolveStatus")
        .value("kNotPresolved", HighsPresolveStatus::kNotPresolved)
        .value("kNotReduced", HighsPresolveStatus::kNotReduced)
        .value("kInfeasible", HighsPresolveStatus::kInfeasible)
        .value("kUnboundedOrInfeasible",
               HighsPresolveStatus::kUnboundedOrInfeasible)
        .value("kReduced", HighsPresolveStatus::kReduced)
        .value("kReducedToEmpty", HighsPresolveStatus::kReducedToEmpty)
        .value("kTimeout", HighsPresolveStatus::kTimeout)
        .value("kNullError", HighsPresolveStatus::kNullError)
        .value("kOptionsError", HighsPresolveStatus::kOptionsError);
    // ;
    py::enum_<HighsBasisStatus>("HighsBasisStatus")
        .value("kLower", HighsBasisStatus::kLower)
        .value("kBasic", HighsBasisStatus::kBasic)
        .value("kUpper", HighsBasisStatus::kUpper)
        .value("kZero", HighsBasisStatus::kZero)
        .value("kNonbasic", HighsBasisStatus::kNonbasic);
    // ;
    py::enum_<HighsVarType>("HighsVarType")
        .value("kContinuous", HighsVarType::kContinuous)
        .value("kInteger", HighsVarType::kInteger)
        .value("kSemiContinuous", HighsVarType::kSemiContinuous)
        .value("kSemiInteger", HighsVarType::kSemiInteger);
    // ;
    py::enum_<HighsOptionType>("HighsOptionType")
        .value("kBool", HighsOptionType::kBool)
        .value("kInt", HighsOptionType::kInt)
        .value("kDouble", HighsOptionType::kDouble)
        .value("kString", HighsOptionType::kString);
    // ;
    py::enum_<HighsInfoType>("HighsInfoType")
        .value("kInt64", HighsInfoType::kInt64)
        .value("kInt", HighsInfoType::kInt)
        .value("kDouble", HighsInfoType::kDouble);
    // ;
    py::enum_<HighsStatus>("HighsStatus")
        .value("kError", HighsStatus::kError)
        .value("kOk", HighsStatus::kOk)
        .value("kWarning", HighsStatus::kWarning);
    py::enum_<HighsLogType>("HighsLogType")
        .value("kInfo", HighsLogType::kInfo)
        .value("kDetailed", HighsLogType::kDetailed)
        .value("kVerbose", HighsLogType::kVerbose)
        .value("kWarning", HighsLogType::kWarning)
        .value("kError", HighsLogType::kError);
    // ;
    py::enum_<IisStrategy>("IisStrategy")
        .value("kIisStrategyMin", IisStrategy::kIisStrategyMin)
        .value("kIisStrategyFromLpRowPriority",
               IisStrategy::kIisStrategyFromLpRowPriority)
        .value("kIisStrategyFromLpColPriority",
               IisStrategy::kIisStrategyFromLpColPriority)
        .value("kIisStrategyMax", IisStrategy::kIisStrategyMax);
    // ;
    py::enum_<IisBoundStatus>("IisBoundStatus")
        .value("kIisBoundStatusDropped", IisBoundStatus::kIisBoundStatusDropped)
        .value("kIisBoundStatusNull", IisBoundStatus::kIisBoundStatusNull)
        .value("kIisBoundStatusFree", IisBoundStatus::kIisBoundStatusFree)
        .value("kIisBoundStatusLower", IisBoundStatus::kIisBoundStatusLower)
        .value("kIisBoundStatusUpper", IisBoundStatus::kIisBoundStatusUpper)
        .value("kIisBoundStatusBoxed", IisBoundStatus::kIisBoundStatusBoxed);
    // ;
    // Classes
    py::class_<HighsSparseMatrix>("HighsSparseMatrix")
        .constructor()
        .def_readwrite("format_", &HighsSparseMatrix::format_)
        .def_readwrite("num_col_", &HighsSparseMatrix::num_col_)
        .def_readwrite("num_row_", &HighsSparseMatrix::num_row_)
        .def_readwrite("start_", &HighsSparseMatrix::start_)
        .def_readwrite("p_end_", &HighsSparseMatrix::p_end_)
        .def_readwrite("index_", &HighsSparseMatrix::index_)
        .def_readwrite("value_", &HighsSparseMatrix::value_);
    py::class_<HighsLpMods>("HighsLpMods");
    py::class_<HighsScale>("HighsScale");
    py::class_<HighsLp>("HighsLp")
        .constructor()
        .def_readwrite("num_col_", &HighsLp::num_col_)
        .def_readwrite("num_row_", &HighsLp::num_row_)
        .def_readwrite("col_cost_", &HighsLp::col_cost_)
        .def_readwrite("col_lower_", &HighsLp::col_lower_)
        .def_readwrite("col_upper_", &HighsLp::col_upper_)
        .def_readwrite("row_lower_", &HighsLp::row_lower_)
        .def_readwrite("row_upper_", &HighsLp::row_upper_)
        .def_readwrite("a_matrix_", &HighsLp::a_matrix_)
        .def_readwrite("sense_", &HighsLp::sense_)
        .def_readwrite("offset_", &HighsLp::offset_)
        .def_readwrite("model_name_", &HighsLp::model_name_)
        .def_readwrite("col_names_", &HighsLp::col_names_)
        .def_readwrite("row_names_", &HighsLp::row_names_)
        .def_readwrite("integrality_", &HighsLp::integrality_)
        .def_readwrite("scale_", &HighsLp::scale_)
        .def_readwrite("is_scaled_", &HighsLp::is_scaled_)
        .def_readwrite("is_moved_", &HighsLp::is_moved_)
        .def_readwrite("mods_", &HighsLp::mods_);
    py::class_<HighsHessian>("HighsHessian")
        .constructor()
        .def_readwrite("dim_", &HighsHessian::dim_)
        .def_readwrite("format_", &HighsHessian::format_)
        .def_readwrite("start_", &HighsHessian::start_)
        .def_readwrite("index_", &HighsHessian::index_)
        .def_readwrite("value_", &HighsHessian::value_);
    py::class_<HighsModel>("HighsModel")
        .constructor()
        .def_readwrite("lp_", &HighsModel::lp_)
        .def_readwrite("hessian_", &HighsModel::hessian_);
    py::class_<HighsInfo>("HighsInfo")
        .constructor()
        .property("records", &HighsInfo::records);
    //   .def_readwrite("valid", &HighsInfo::valid)
    //   .def_readwrite("mip_node_count", &HighsInfo::mip_node_count)
    //   .def_readwrite("simplex_iteration_count",
    //                  &HighsInfo::simplex_iteration_count)
    //   .def_readwrite("ipm_iteration_count", &HighsInfo::ipm_iteration_count)
    //   .def_readwrite("qp_iteration_count", &HighsInfo::qp_iteration_count)
    //   .def_readwrite("crossover_iteration_count",
    //                  &HighsInfo::crossover_iteration_count)
    //   .def_readwrite("pdlp_iteration_count", &HighsInfo::pdlp_iteration_count)
    //   .def_readwrite("primal_solution_status",
    //                  &HighsInfo::primal_solution_status)
    //   .def_readwrite("dual_solution_status", &HighsInfo::dual_solution_status)
    //   .def_readwrite("basis_validity", &HighsInfo::basis_validity)
    //   .def_readwrite("objective_function_value",
    //                  &HighsInfo::objective_function_value)
    //   .def_readwrite("mip_dual_bound", &HighsInfo::mip_dual_bound)
    //   .def_readwrite("mip_gap", &HighsInfo::mip_gap)
    //   .def_readwrite("max_integrality_violation",
    //                  &HighsInfo::max_integrality_violation)
    //   .def_readwrite("num_primal_infeasibilities",
    //                  &HighsInfo::num_primal_infeasibilities)
    //   .def_readwrite("max_primal_infeasibility",
    //                  &HighsInfo::max_primal_infeasibility)
    //   .def_readwrite("sum_primal_infeasibilities",
    //                  &HighsInfo::sum_primal_infeasibilities)
    //   .def_readwrite("num_dual_infeasibilities",
    //                  &HighsInfo::num_dual_infeasibilities)
    //   .def_readwrite("max_dual_infeasibility",
    //                  &HighsInfo::max_dual_infeasibility)
    //   .def_readwrite("sum_dual_infeasibilities",
    //                  &HighsInfo::sum_dual_infeasibilities)
    //   .def_readwrite("max_complementarity_violation",
    //                  &HighsInfo::max_complementarity_violation)
    //   .def_readwrite("sum_complementarity_violations",
    //                  &HighsInfo::sum_complementarity_violations);
    py::class_<HighsOptions>("HighsOptions")
        .constructor()
        .property("records", &HighsOptions::records);
    //   .def_readwrite("presolve", &HighsOptions::presolve)
    //   .def_readwrite("solver", &HighsOptions::solver)
    //   .def_readwrite("parallel", &HighsOptions::parallel)
    //   .def_readwrite("run_crossover", &HighsOptions::run_crossover)
    //   .def_readwrite("ranging", &HighsOptions::ranging)
    //   .def_readwrite("time_limit", &HighsOptions::time_limit)
    //   .def_readwrite("infinite_cost", &HighsOptions::infinite_cost)
    //   .def_readwrite("infinite_bound", &HighsOptions::infinite_bound)
    //   .def_readwrite("small_matrix_value", &HighsOptions::small_matrix_value)
    //   .def_readwrite("large_matrix_value", &HighsOptions::large_matrix_value)
    //   .def_readwrite("primal_feasibility_tolerance",
    //                  &HighsOptions::primal_feasibility_tolerance)
    //   .def_readwrite("dual_feasibility_tolerance",
    //                  &HighsOptions::dual_feasibility_tolerance)
    //   .def_readwrite("ipm_optimality_tolerance",
    //                  &HighsOptions::ipm_optimality_tolerance)
    //   .def_readwrite("objective_bound", &HighsOptions::objective_bound)
    //   .def_readwrite("objective_target", &HighsOptions::objective_target)
    //   .def_readwrite("random_seed", &HighsOptions::random_seed)
    //   .def_readwrite("threads", &HighsOptions::threads)
    //   .def_readwrite("highs_debug_level", &HighsOptions::highs_debug_level)
    //   .def_readwrite("highs_analysis_level",
    //                  &HighsOptions::highs_analysis_level)
    //   .def_readwrite("simplex_strategy", &HighsOptions::simplex_strategy)
    //   .def_readwrite("simplex_scale_strategy",
    //                  &HighsOptions::simplex_scale_strategy)
    //   .def_readwrite("simplex_crash_strategy",
    //                  &HighsOptions::simplex_crash_strategy)
    //   .def_readwrite("simplex_dual_edge_weight_strategy",
    //                  &HighsOptions::simplex_dual_edge_weight_strategy)
    //   .def_readwrite("simplex_primal_edge_weight_strategy",
    //                  &HighsOptions::simplex_primal_edge_weight_strategy)
    //   .def_readwrite("simplex_iteration_limit",
    //                  &HighsOptions::simplex_iteration_limit)
    //   .def_readwrite("simplex_update_limit",
    //                  &HighsOptions::simplex_update_limit)
    //   .def_readwrite("simplex_min_concurrency",
    //                  &HighsOptions::simplex_min_concurrency)
    //   .def_readwrite("simplex_max_concurrency",
    //                  &HighsOptions::simplex_max_concurrency)
    //   .def_readwrite("ipm_iteration_limit", &HighsOptions::ipm_iteration_limit)
    //   .def_readwrite("write_model_file", &HighsOptions::write_model_file)
    //   .def_readwrite("solution_file", &HighsOptions::solution_file)
    //   .def_readwrite("log_file", &HighsOptions::log_file)
    //   .def_readwrite("write_model_to_file", &HighsOptions::write_model_to_file)
    //   .def_readwrite("write_solution_to_file",
    //                  &HighsOptions::write_solution_to_file)
    //   .def_readwrite("write_solution_style",
    //                  &HighsOptions::write_solution_style)
    //   .def_readwrite("output_flag", &HighsOptions::output_flag)
    //   .def_readwrite("log_to_console", &HighsOptions::log_to_console)
    //   .def_readwrite("log_dev_level", &HighsOptions::log_dev_level)
    //   .def_readwrite("allow_unbounded_or_infeasible",
    //                  &HighsOptions::allow_unbounded_or_infeasible)
    //   .def_readwrite("allowed_matrix_scale_factor",
    //                  &HighsOptions::allowed_matrix_scale_factor)
    //   .def_readwrite("ipx_dualize_strategy",
    //                  &HighsOptions::ipx_dualize_strategy)
    //   .def_readwrite("simplex_dualize_strategy",
    //                  &HighsOptions::simplex_dualize_strategy)
    //   .def_readwrite("simplex_permute_strategy",
    //                  &HighsOptions::simplex_permute_strategy)
    //   .def_readwrite("simplex_price_strategy",
    //                  &HighsOptions::simplex_price_strategy)
    //   .def_readwrite("mip_detect_symmetry", &HighsOptions::mip_detect_symmetry)
    //   .def_readwrite("mip_max_nodes", &HighsOptions::mip_max_nodes)
    //   .def_readwrite("mip_max_stall_nodes", &HighsOptions::mip_max_stall_nodes)
    //   .def_readwrite("mip_max_leaves", &HighsOptions::mip_max_leaves)
    //   .def_readwrite("mip_max_improving_sols",
    //                  &HighsOptions::mip_max_improving_sols)
    //   .def_readwrite("mip_lp_age_limit", &HighsOptions::mip_lp_age_limit)
    //   .def_readwrite("mip_pool_age_limit", &HighsOptions::mip_pool_age_limit)
    //   .def_readwrite("mip_pool_soft_limit", &HighsOptions::mip_pool_soft_limit)
    //   .def_readwrite("mip_pscost_minreliable",
    //                  &HighsOptions::mip_pscost_minreliable)
    //   .def_readwrite("mip_min_cliquetable_entries_for_parallelism",
    //                  &HighsOptions::mip_min_cliquetable_entries_for_parallelism)
    //   .def_readwrite("mip_report_level", &HighsOptions::mip_report_level)
    //   .def_readwrite("mip_feasibility_tolerance",
    //                  &HighsOptions::mip_feasibility_tolerance)
    //   .def_readwrite("mip_rel_gap", &HighsOptions::mip_rel_gap)
    //   .def_readwrite("mip_abs_gap", &HighsOptions::mip_abs_gap)
    //   .def_readwrite("mip_heuristic_effort",
    //                  &HighsOptions::mip_heuristic_effort)
    //   .def_readwrite("mip_min_logging_interval",
    //                  &HighsOptions::mip_min_logging_interval);
    py::class_<Highs>("Highs")
        .constructor()
        .property("version", &Highs::version)
        .property("versionMajor", &Highs::versionMajor)
        .property("versionMinor", &Highs::versionMinor)
        .property("versionPatch", &Highs::versionPatch)
        .property("githash", &Highs::githash)
        .function("clear", &Highs::clear)
        .function("clearModel", &Highs::clearModel)
        .function("clearSolver", &Highs::clearSolver)
        .function("passModel", &highs_passModel)
        .function("passModel", &highs_passModelPointers)
        .function("passLp", &highs_passLp)
        .function("passLp", &highs_passLpPointers)
        .function("passHessian", &highs_passHessian)
        .function("passHessian", &highs_passHessianPointers)
        .function("passColName", &Highs::passColName)
        .function("passRowName", &Highs::passRowName)
        .function("readModel", &Highs::readModel)
        .function("readBasis", &Highs::readBasis)
        .function("writeBasis", &Highs::writeBasis)
        .function("postsolve", &highs_postsolve)
        .function("postsolve", &highs_mipPostsolve)
        .function("run", &Highs::run)
        .class_function("resetGlobalScheduler", &Highs::resetGlobalScheduler)
        .def(
          "feasibilityRelaxation", &highs_feasibilityRelaxation
          )
        .function("getIis", &Highs::getIis)
        .function("presolve", &Highs::presolve /*, py::call_guard<py::gil_scoped_release>()*/)
        .function("writeSolution", &highs_writeSolution) // TODO: check what is done here in Python bindings
        .function("readSolution", &Highs::readSolution)
        .function("setOptionValue", &highs_setOptionValue)
                //   static_cast<HighsStatus (Highs::*)(const std::string &, const bool)>(
                //       &Highs::setOptionValue))
        // .function("setOptionValue",
        //           static_cast<HighsStatus (Highs::*)(const std::string &, const int)>(
        //               &Highs::setOptionValue))
        // .function(
        //     "setOptionValue",
        //     static_cast<HighsStatus (Highs::*)(const std::string &, const double)>(
        //         &Highs::setOptionValue))
        // .function("setOptionValue",
        //           static_cast<HighsStatus (Highs::*)(
        //               const std::string &, const std::string &)>(&Highs::setOptionValue))
        .function("readOptions", &Highs::readOptions)
        .function("passOptions", &Highs::passOptions)
        .function("getOptions", &Highs::getOptions)
        .function("getOptionValue", &highs_getOptionValue) // TODO: check what is done here in py bindings
        //    .def("getOptionName", &highs_getOptionName)
        .function("getOptionType", &highs_getOptionType)
        .function("resetOptions", &Highs::resetOptions)
        .function("writeOptions", &highs_writeOptions)
        //    .def("getBoolOptionValues", &highs_getBoolOptionValues)
        //    .def("getIntOptionValues", &highs_getIntOptionValues)
        //    .def("getDoubleOptionValues", &highs_getDoubleOptionValues)
        //    .def("getStringOptionValues", &highs_getStringOptionValues)
        .function("getInfo", &Highs::getInfo)
        .function("getInfoValue", &highs_getInfoValue)
        .function("getInfoType", &highs_getInfoType)
        .function("writeInfo", &Highs::writeInfo)
        .function("getInfinity", &Highs::getInfinity)
        .function("getRunTime", &Highs::getRunTime)
        .function("getPresolvedLp", &Highs::getPresolvedLp)
        //    .def("getPresolvedModel", &Highs::getPresolvedModel)
        //    .def("getPresolveLog", &Highs::getPresolveLog)
        .function("getLp", &Highs::getLp)
        .function("getModel", &Highs::getModel)
        .function("getSolution", &Highs::getSolution)
        .function("getSavedMipSolutions", &Highs::getSavedMipSolutions)
        .function("getBasis", &Highs::getBasis)
        // &highs_getModelStatus not needed once getModelStatus(const bool
        // scaled_model) disappears froHighs.h
        .function("getModelStatus", &highs_getModelStatus) //&Highs::getModelStatus)
        .function("getModelPresolveStatus", &Highs::getModelPresolveStatus)
        .function("getRanging", &highs_getRanging)
        .function("getObjectiveValue", &Highs::getObjectiveValue)
        .function("getNumCol", &Highs::getNumCol)
        .function("getNumRow", &Highs::getNumRow)
        .function("getNumNz", &Highs::getNumNz)
        .function("getHessianNumNz", &Highs::getHessianNumNz)
        .function("getObjectiveSense", &highs_getObjectiveSense)
        .function("getObjectiveOffset", &highs_getObjectiveOffset)

        .function("getCol", &highs_getCol)
        .function("getColEntries", &highs_getColEntries)
        .function("getRow", &highs_getRow)
        .function("getRowEntries", &highs_getRowEntries)

        .function("getCols", &highs_getCols)
        .function("getColsEntries", &highs_getColsEntries)
        .function("getRows", &highs_getRows)
        .function("getRowsEntries", &highs_getRowsEntries)

        .function("getColName", &highs_getColName)
        .function("getColByName", &highs_getColByName)
        .function("getRowName", &highs_getRowName)
        .function("getRowByName", &highs_getRowByName)

        .function("writeModel", &Highs::writeModel)
        .function("writePresolvedModel", &Highs::writePresolvedModel)
        .function("crossover", &Highs::crossover)
        .function("changeObjectiveSense", &Highs::changeObjectiveSense)
        .function("changeObjectiveOffset", &Highs::changeObjectiveOffset)
        .function("changeColIntegrality", &Highs::changeColIntegrality)
        .function("changeColCost", &Highs::changeColCost)
        .function("changeColBounds", &Highs::changeColBounds)
        .function("changeRowBounds", &Highs::changeRowBounds)
        .function("changeCoeff", &Highs::changeCoeff)
        .function("addRows", &highs_addRows)
        .function("addRow", &highs_addRow)
        .function("addCol", &highs_addCol)
        .function("addCols", &highs_addCols)
        .function("addVar", &highs_addVar)
        .function("addVars", &highs_addVars)
        .function("changeColsCost", &highs_changeColsCost)
        .function("changeColsBounds", &highs_changeColsBounds)
        .function("changeColsIntegrality", &highs_changeColsIntegrality)
        .function("deleteCols", &highs_deleteCols)
        .function("deleteVars", &highs_deleteCols) // alias
        .function("deleteRows", &highs_deleteRows)
        .function("setSolution", &highs_setSolution)
        .function("setSolution", &highs_setSparseSolution)
        .function("setBasis", &highs_setBasis)
        .function("setBasis", &highs_setLogicalBasis)
        .function("modelStatusToString", &Highs::modelStatusToString)
        .function("solutionStatusToString", &Highs::solutionStatusToString)
        .function("basisStatusToString", &Highs::basisStatusToString)
        .function("basisValidityToString", &Highs::basisValidityToString)
        .function("setCallback", &highs_setCallback)
        .function("startCallback",
                  static_cast<HighsStatus (Highs::*)(const HighsCallbackType)>(
                      &Highs::startCallback))
        .function("stopCallback",
                  static_cast<HighsStatus (Highs::*)(const HighsCallbackType)>(
                      &Highs::stopCallback))
        .function("startCallbackInt", static_cast<HighsStatus (Highs::*)(const int)>(
                                          &Highs::startCallback))
        .function("stopCallbackInt", static_cast<HighsStatus (Highs::*)(const int)>(
                                         &Highs::stopCallback));
    ;

    py::class_<HighsIis>("HighsIis")
        .constructor()
        .def("invalidate", &HighsIis::invalidate)
        .def_readwrite("valid", &HighsIis::valid_)
        .def_readwrite("strategy", &HighsIis::strategy_)
        .def_readwrite("col_index", &HighsIis::col_index_)
        .def_readwrite("row_index", &HighsIis::row_index_)
        .def_readwrite("col_bound", &HighsIis::col_bound_)
        .def_readwrite("row_bound", &HighsIis::row_bound_)
        .def_readwrite("info", &HighsIis::info_);
    // structs
    py::class_<HighsSolution>("HighsSolution")
        .constructor()
        .def_readwrite("value_valid", &HighsSolution::value_valid)
        .def_readwrite("dual_valid", &HighsSolution::dual_valid)
        .def_readwrite("col_value", &HighsSolution::col_value)
        .def_readwrite("col_dual", &HighsSolution::col_dual)
        .def_readwrite("row_value", &HighsSolution::row_value)
        .def_readwrite("row_dual", &HighsSolution::row_dual);
    py::class_<HighsObjectiveSolution>("HighsObjectiveSolution")
        .constructor()
        .def_readwrite("objective", &HighsObjectiveSolution::objective)
        .def_readwrite("col_value", &HighsObjectiveSolution::col_value);
    py::class_<HighsBasis>("HighsBasis")
        .constructor()
        .def_readwrite("valid", &HighsBasis::valid)
        .def_readwrite("alien", &HighsBasis::alien)
        .def_readwrite("was_alien", &HighsBasis::was_alien)
        .def_readwrite("debug_id", &HighsBasis::debug_id)
        .def_readwrite("debug_update_count", &HighsBasis::debug_update_count)
        .def_readwrite("debug_origin_name", &HighsBasis::debug_origin_name)
        .def_readwrite("col_status", &HighsBasis::col_status)
        .def_readwrite("row_status", &HighsBasis::row_status);
    py::class_<HighsRangingRecord>("HighsRangingRecord")
        .constructor()
        .def_readwrite("value_", &HighsRangingRecord::value_)
        .def_readwrite("objective_", &HighsRangingRecord::objective_)
        .def_readwrite("in_var_", &HighsRangingRecord::in_var_)
        .def_readwrite("ou_var_", &HighsRangingRecord::ou_var_);
    py::class_<HighsRanging>("HighsRanging")
        .constructor()
        .def_readwrite("valid", &HighsRanging::valid)
        .def_readwrite("col_cost_up", &HighsRanging::col_cost_up)
        .def_readwrite("col_cost_dn", &HighsRanging::col_cost_dn)
        .def_readwrite("col_bound_up", &HighsRanging::col_bound_up)
        .def_readwrite("col_bound_dn", &HighsRanging::col_bound_dn)
        .def_readwrite("row_bound_up", &HighsRanging::row_bound_up)
        .def_readwrite("row_bound_dn", &HighsRanging::row_bound_dn);
    py::class_<HighsIisInfo>("HighsIisInfo")
        .constructor()
        .def_readwrite("simplex_time", &HighsIisInfo::simplex_time)
        .def_readwrite("simplex_iterations", &HighsIisInfo::simplex_iterations);
    // constants
    emscripten::constant("kHighsInf", kHighsInf);
    emscripten::constant("kHighsIInf", kHighsIInf);

    emscripten::constant("HIGHS_VERSION_MAJOR", HIGHS_VERSION_MAJOR);
    emscripten::constant("HIGHS_VERSION_MINOR", HIGHS_VERSION_MINOR);
    emscripten::constant("HIGHS_VERSION_PATCH", HIGHS_VERSION_PATCH);

    // Submodules
    py::enum_<SimplexStrategy>("SimplexStrategy")
        .value("kSimplexStrategyMin", SimplexStrategy::kSimplexStrategyMin)
        .value("kSimplexStrategyChoose", SimplexStrategy::kSimplexStrategyChoose)
        .value("kSimplexStrategyDual", SimplexStrategy::kSimplexStrategyDual)
        .value("kSimplexStrategyDualPlain",
               SimplexStrategy::kSimplexStrategyDualPlain)
        .value("kSimplexStrategyDualTasks",
               SimplexStrategy::kSimplexStrategyDualTasks)
        .value("kSimplexStrategyDualMulti",
               SimplexStrategy::kSimplexStrategyDualMulti)
        .value("kSimplexStrategyPrimal", SimplexStrategy::kSimplexStrategyPrimal)
        .value("kSimplexStrategyMax", SimplexStrategy::kSimplexStrategyMax)
        .value("kSimplexStrategyNum", SimplexStrategy::kSimplexStrategyNum); // needed since it isn't an enum class
    py::enum_<SimplexUnscaledSolutionStrategy>(
        "SimplexUnscaledSolutionStrategy")
        .value(
            "kSimplexUnscaledSolutionStrategyMin",
            SimplexUnscaledSolutionStrategy::kSimplexUnscaledSolutionStrategyMin)
        .value(
            "kSimplexUnscaledSolutionStrategyNone",
            SimplexUnscaledSolutionStrategy::kSimplexUnscaledSolutionStrategyNone)
        .value("kSimplexUnscaledSolutionStrategyRefine",
               SimplexUnscaledSolutionStrategy::
                   kSimplexUnscaledSolutionStrategyRefine)
        .value("kSimplexUnscaledSolutionStrategyDirect",
               SimplexUnscaledSolutionStrategy::
                   kSimplexUnscaledSolutionStrategyDirect)
        .value(
            "kSimplexUnscaledSolutionStrategyMax",
            SimplexUnscaledSolutionStrategy::kSimplexUnscaledSolutionStrategyMax)
        .value(
            "kSimplexUnscaledSolutionStrategyNum",
            SimplexUnscaledSolutionStrategy::kSimplexUnscaledSolutionStrategyNum);
    py::enum_<SimplexSolvePhase>("SimplexSolvePhase")
        .value("kSolvePhaseMin", SimplexSolvePhase::kSolvePhaseMin)
        .value("kSolvePhaseError", SimplexSolvePhase::kSolvePhaseError)
        .value("kSolvePhaseExit", SimplexSolvePhase::kSolvePhaseExit)
        .value("kSolvePhaseUnknown", SimplexSolvePhase::kSolvePhaseUnknown)
        .value("kSolvePhaseOptimal", SimplexSolvePhase::kSolvePhaseOptimal)
        .value("kSolvePhase1", SimplexSolvePhase::kSolvePhase1)
        .value("kSolvePhase2", SimplexSolvePhase::kSolvePhase2)
        .value("kSolvePhasePrimalInfeasibleCleanup",
               SimplexSolvePhase::kSolvePhasePrimalInfeasibleCleanup)
        .value("kSolvePhaseOptimalCleanup",
               SimplexSolvePhase::kSolvePhaseOptimalCleanup)
        .value("kSolvePhaseTabooBasis", SimplexSolvePhase::kSolvePhaseTabooBasis)
        .value("kSolvePhaseMax", SimplexSolvePhase::kSolvePhaseMax);
    py::enum_<SimplexEdgeWeightStrategy>(
        "SimplexEdgeWeightStrategy")
        .value("kSimplexEdgeWeightStrategyMin",
               SimplexEdgeWeightStrategy::kSimplexEdgeWeightStrategyMin)
        .value("kSimplexEdgeWeightStrategyChoose",
               SimplexEdgeWeightStrategy::kSimplexEdgeWeightStrategyChoose)
        .value("kSimplexEdgeWeightStrategyDantzig",
               SimplexEdgeWeightStrategy::kSimplexEdgeWeightStrategyDantzig)
        .value("kSimplexEdgeWeightStrategyDevex",
               SimplexEdgeWeightStrategy::kSimplexEdgeWeightStrategyDevex)
        .value("kSimplexEdgeWeightStrategySteepestEdge",
               SimplexEdgeWeightStrategy::kSimplexEdgeWeightStrategySteepestEdge)
        .value("kSimplexEdgeWeightStrategyMax",
               SimplexEdgeWeightStrategy::kSimplexEdgeWeightStrategyMax);
    py::enum_<SimplexPriceStrategy>("SimplexPriceStrategy")
        .value("kSimplexPriceStrategyMin",
               SimplexPriceStrategy::kSimplexPriceStrategyMin)
        .value("kSimplexPriceStrategyCol",
               SimplexPriceStrategy::kSimplexPriceStrategyCol)
        .value("kSimplexPriceStrategyRow",
               SimplexPriceStrategy::kSimplexPriceStrategyRow)
        .value("kSimplexPriceStrategyRowSwitch",
               SimplexPriceStrategy::kSimplexPriceStrategyRowSwitch)
        .value("kSimplexPriceStrategyRowSwitchColSwitch",
               SimplexPriceStrategy::kSimplexPriceStrategyRowSwitchColSwitch)
        .value("kSimplexPriceStrategyMax",
               SimplexPriceStrategy::kSimplexPriceStrategyMax);
    py::enum_<SimplexPivotalRowRefinementStrategy>(
        "SimplexPivotalRowRefinementStrategy")
        .value("kSimplexInfeasibilityProofRefinementMin",
               SimplexPivotalRowRefinementStrategy::
                   kSimplexInfeasibilityProofRefinementMin)
        .value("kSimplexInfeasibilityProofRefinementNo",
               SimplexPivotalRowRefinementStrategy::
                   kSimplexInfeasibilityProofRefinementNo)
        .value("kSimplexInfeasibilityProofRefinementUnscaledLp",
               SimplexPivotalRowRefinementStrategy::
                   kSimplexInfeasibilityProofRefinementUnscaledLp)
        .value("kSimplexInfeasibilityProofRefinementAlsoScaledLp",
               SimplexPivotalRowRefinementStrategy::
                   kSimplexInfeasibilityProofRefinementAlsoScaledLp)
        .value("kSimplexInfeasibilityProofRefinementMax",
               SimplexPivotalRowRefinementStrategy::
                   kSimplexInfeasibilityProofRefinementMax);
    py::enum_<SimplexPrimalCorrectionStrategy>(
        "SimplexPrimalCorrectionStrategy")
        .value(
            "kSimplexPrimalCorrectionStrategyNone",
            SimplexPrimalCorrectionStrategy::kSimplexPrimalCorrectionStrategyNone)
        .value("kSimplexPrimalCorrectionStrategyInRebuild",
               SimplexPrimalCorrectionStrategy::
                   kSimplexPrimalCorrectionStrategyInRebuild)
        .value("kSimplexPrimalCorrectionStrategyAlways",
               SimplexPrimalCorrectionStrategy::
                   kSimplexPrimalCorrectionStrategyAlways);
    py::enum_<SimplexNlaOperation>("SimplexNlaOperation")
        .value("kSimplexNlaNull", SimplexNlaOperation::kSimplexNlaNull)
        .value("kSimplexNlaBtranFull", SimplexNlaOperation::kSimplexNlaBtranFull)
        .value("kSimplexNlaPriceFull", SimplexNlaOperation::kSimplexNlaPriceFull)
        .value("kSimplexNlaBtranBasicFeasibilityChange",
               SimplexNlaOperation::kSimplexNlaBtranBasicFeasibilityChange)
        // .value("kSimplexNlaPriceBasicFeasibilityChange",
        //        /khighsSimplexNlaOperation::kSimplexNlaPriceBasicFeasibilityChange)
        .value("kSimplexNlaBtranEp", SimplexNlaOperation::kSimplexNlaBtranEp)
        .value("kSimplexNlaPriceAp", SimplexNlaOperation::kSimplexNlaPriceAp)
        .value("kSimplexNlaFtran", SimplexNlaOperation::kSimplexNlaFtran)
        .value("kSimplexNlaFtranBfrt", SimplexNlaOperation::kSimplexNlaFtranBfrt)
        .value("kSimplexNlaFtranDse", SimplexNlaOperation::kSimplexNlaFtranDse)
        .value("kSimplexNlaBtranPse", SimplexNlaOperation::kSimplexNlaBtranPse)
        .value("kNumSimplexNlaOperation",
               SimplexNlaOperation::kNumSimplexNlaOperation);
    py::enum_<EdgeWeightMode>("EdgeWeightMode")
        .value("kDantzig", EdgeWeightMode::kDantzig)
        .value("kDevex", EdgeWeightMode::kDevex)
        .value("kSteepestEdge", EdgeWeightMode::kSteepestEdge)
        .value("kCount", EdgeWeightMode::kCount);

    // Types for interface
    py::enum_<HighsCallbackType>("HighsCallbackType")
        .value("kCallbackMin", HighsCallbackType::kCallbackMin)
        .value("kCallbackLogging", HighsCallbackType::kCallbackLogging)
        .value("kCallbackSimplexInterrupt",
               HighsCallbackType::kCallbackSimplexInterrupt)
        .value("kCallbackIpmInterrupt", HighsCallbackType::kCallbackIpmInterrupt)
        .value("kCallbackMipSolution", HighsCallbackType::kCallbackMipSolution)
        .value("kCallbackMipImprovingSolution",
               HighsCallbackType::kCallbackMipImprovingSolution)
        .value("kCallbackMipLogging", HighsCallbackType::kCallbackMipLogging)
        .value("kCallbackMipInterrupt", HighsCallbackType::kCallbackMipInterrupt)
        .value("kCallbackMipGetCutPool",
               HighsCallbackType::kCallbackMipGetCutPool)
        .value("kCallbackMipDefineLazyConstraints",
               HighsCallbackType::kCallbackMipDefineLazyConstraints)
        .value("kCallbackMax", HighsCallbackType::kCallbackMax)
        .value("kNumCallbackType", HighsCallbackType::kNumCallbackType);
    // Classes
    //   py::class_<readonly_ptr_wrapper<double>>("readonly_ptr_wrapper_double")
    //       .constructor<double*>()
    //       .def("__getitem__", &readonly_ptr_wrapper<double>::operator[])
    //       .def("__bool__", &readonly_ptr_wrapper<double>::is_valid)
    //       .def("to_array", &readonly_ptr_wrapper<double>::to_array);
    py::class_<HighsCallbackDataOut>("HighsCallbackDataOut")
        .constructor()
        .def_readwrite("log_type", &HighsCallbackDataOut::log_type)
        .def_readwrite("running_time", &HighsCallbackDataOut::running_time)
        .def_readwrite("simplex_iteration_count",
                       &HighsCallbackDataOut::simplex_iteration_count)
        .def_readwrite("ipm_iteration_count",
                       &HighsCallbackDataOut::ipm_iteration_count)
        .def_readwrite("pdlp_iteration_count",
                       &HighsCallbackDataOut::pdlp_iteration_count)
        .def_readwrite("objective_function_value",
                       &HighsCallbackDataOut::objective_function_value)
        .def_readwrite("mip_node_count", &HighsCallbackDataOut::mip_node_count)
        .def_readwrite("mip_primal_bound",
                       &HighsCallbackDataOut::mip_primal_bound)
        .def_readwrite("mip_dual_bound", &HighsCallbackDataOut::mip_dual_bound)
        .def_readwrite("mip_gap", &HighsCallbackDataOut::mip_gap);
    //   .def_property_readonly(
    //       "mip_solution",
    //       [](const HighsCallbackDataOut& self) -> readonly_ptr_wrapper<double> {
    //         return readonly_ptr_wrapper<double>(self.mip_solution);
    //       });
    py::class_<HighsCallbackDataIn>("HighsCallbackDataIn")
        .constructor()
        .def_readwrite("user_interrupt", &HighsCallbackDataIn::user_interrupt);
#pragma endregion
}