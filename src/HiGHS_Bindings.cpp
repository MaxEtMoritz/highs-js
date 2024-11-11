#include <emscripten/bind.h>

#include <Highs.h>

using namespace emscripten;

EMSCRIPTEN_BINDINGS(Highs)
{
    class_<Highs>("Highs")
        .constructor()
        .property("version", &Highs::version)
        .property("versionMajor", &Highs::versionMajor)
        .property("versionMinor", &Highs::versionMinor)
        .property("versionPatch", &Highs::versionPatch)
        .property("githash", &Highs::githash)
        .function("clear", &Highs::clear)
        .function("clearModel", &Highs::clearModel)
        .function("clearSolver", &Highs::clearSolver)
    //     .function("passModel", select_overload<HighsStatus(HighsModel)>(&Highs::passModel))
    //     .function("passModel", select_overload<HighsStatus(HighsLp)>(&Highs::passModel))
    //     // .function("passModel", select_overload<HighsStatus(const HighsInt num_col, const HighsInt num_row, const HighsInt num_nz,
    //     //                                                    const HighsInt q_num_nz, const HighsInt a_format, const HighsInt q_format,
    //     //                                                    const HighsInt sense, const double offset, const double *col_cost,
    //     //                                                    const double *col_lower, const double *col_upper, const double *row_lower,
    //     //                                                    const double *row_upper, const HighsInt *a_start, const HighsInt *a_index,
    //     //                                                    const double *a_value, const HighsInt *q_start, const HighsInt *q_index,
    //     //                                                    const double *q_value, const HighsInt *integrality)>(&Highs::passModel)) // pointer handling in python bindings here
    //     // .function("passModel", select_overload<HighsStatus(const HighsInt num_col, const HighsInt num_row,
    //     //                                                    const HighsInt num_nz, const HighsInt a_format,
    //     //                                                    const HighsInt sense, const double offset,
    //     //                                                    const double *col_cost, const double *col_lower,
    //     //                                                    const double *col_upper, const double *row_lower,
    //     //                                                    const double *row_upper, const HighsInt *a_start,
    //     //                                                    const HighsInt *a_index, const double *a_value,
    //     //                                                    const HighsInt *integrality)>(&Highs::passModel))
    //     .function("passHessian", select_overload<HighsStatus(HighsHessian)>(&Highs::passHessian))
    //     // .function("passHessian", select_overload<HighsStatus(const HighsInt dim, const HighsInt num_nz,
    //     //                                                      const HighsInt format, const HighsInt *start,
    //     //                                                      const HighsInt *index, const double *value)>(&Highs::passHessian))
    //     .function("passColName", &Highs::passColName)
    //     .function("passRowName", &Highs::passRowName)
    //     .function("readModel", &Highs::readModel)
    //     .function("readBasis", &Highs::readBasis)
    //     .function("writeBasis", &Highs::writeBasis)
    //     .function("postsolve", select_overload<HighsStatus(const HighsSolution&)>(&Highs::postsolve))
    //     .function("postsolve", select_overload<HighsStatus(const HighsSolution&, const HighsBasis&)>(&Highs::postsolve))
    //     .function("run", &Highs::run)
    //     .class_function("resetGlobalScheduler", &Highs::resetGlobalScheduler)
    //     //.function("feasibilityRelaxation", &Highs::feasibilityRelaxation) // custom stuff here in python bindings
    //     .function("getIis", &Highs::getIis)
    //   .function("presolve", &Highs::presolve/*, py::call_guard<py::gil_scoped_release>()*/)
    //   //.function("writeSolution", &highs_writeSolution) // TODO: check what is done here in Python bindings
    //   .function("readSolution", &Highs::readSolution)
    //   .function("setOptionValue",
    //        static_cast<HighsStatus (Highs::*)(const std::string&, const bool)>(
    //            &Highs::setOptionValue))
    //   .function("setOptionValue",
    //        static_cast<HighsStatus (Highs::*)(const std::string&, const int)>(
    //            &Highs::setOptionValue))
    //   .function(
    //       "setOptionValue",
    //       static_cast<HighsStatus (Highs::*)(const std::string&, const double)>(
    //           &Highs::setOptionValue))
    //   .function("setOptionValue",
    //        static_cast<HighsStatus (Highs::*)(
    //            const std::string&, const std::string&)>(&Highs::setOptionValue))
    //   .function("readOptions", &Highs::readOptions)
    //   .function("passOptions", &Highs::passOptions)
    //   .function("getOptions", &Highs::getOptions)
    //   //.function("getOptionValue", &highs_getOptionValue) // TODO: check what is done here in py bindings
    //   //    .def("getOptionName", &highs_getOptionName)
    //   //.function("getOptionType", &highs_getOptionType)
    //   .function("resetOptions", &Highs::resetOptions)
    //   //.function("writeOptions", &highs_writeOptions)
    //   //    .def("getBoolOptionValues", &highs_getBoolOptionValues)
    //   //    .def("getIntOptionValues", &highs_getIntOptionValues)
    //   //    .def("getDoubleOptionValues", &highs_getDoubleOptionValues)
    //   //    .def("getStringOptionValues", &highs_getStringOptionValues)
    //   .function("getInfo", &Highs::getInfo)
    //   //.function("getInfoValue", &highs_getInfoValue)
    //   //.function("getInfoType", &highs_getInfoType)
    //   .function("writeInfo", &Highs::writeInfo)
    //   .function("getInfinity", &Highs::getInfinity)
    //   .function("getRunTime", &Highs::getRunTime)
    //   .function("getPresolvedLp", &Highs::getPresolvedLp)
    //   //    .def("getPresolvedModel", &Highs::getPresolvedModel)
    //   //    .def("getPresolveLog", &Highs::getPresolveLog)
    //   .function("getLp", &Highs::getLp)
    //   .function("getModel", &Highs::getModel)
    //   .function("getSolution", &Highs::getSolution)
    //   .function("getSavedMipSolutions", &Highs::getSavedMipSolutions)
    //   .function("getBasis", &Highs::getBasis)
    //   // &highs_getModelStatus not needed once getModelStatus(const bool
    //   // scaled_model) disappears from, Highs.h
    //   //.function("getModelStatus", &highs_getModelStatus)  //&Highs::getModelStatus)
    //   .function("getModelPresolveStatus", &Highs::getModelPresolveStatus)
    //   //.function("getRanging", &highs_getRanging)
    //   .function("getObjectiveValue", &Highs::getObjectiveValue)
    //   .function("getNumCol", &Highs::getNumCol)
    //   .function("getNumRow", &Highs::getNumRow)
    //   .function("getNumNz", &Highs::getNumNz)
    //   .function("getHessianNumNz", &Highs::getHessianNumNz)
    //   //.function("getObjectiveSense", &highs_getObjectiveSense)
    //   //.function("getObjectiveOffset", &highs_getObjectiveOffset)

    //   //.function("getCol", &highs_getCol)
    //   //.function("getColEntries", &highs_getColEntries)
    //   //.function("getRow", &highs_getRow)
    //   //.function("getRowEntries", &highs_getRowEntries)

    //   //.function("getCols", &highs_getCols)
    //   //.function("getColsEntries", &highs_getColsEntries)
    //   //.function("getRows", &highs_getRows)
    //   //.function("getRowsEntries", &highs_getRowsEntries)

    //   //.function("getColName", &highs_getColName)
    //   //.function("getColByName", &highs_getColByName)
    //   //.function("getRowName", &highs_getRowName)
    //   //.function("getRowByName", &highs_getRowByName)

    //   .function("writeModel", &Highs::writeModel)
    //   .function("writePresolvedModel", &Highs::writePresolvedModel)
    //   .function("crossover", &Highs::crossover)
    //   .function("changeObjectiveSense", &Highs::changeObjectiveSense)
    //   .function("changeObjectiveOffset", &Highs::changeObjectiveOffset)
    //   .function("changeColIntegrality", &Highs::changeColIntegrality)
    //   .function("changeColCost", &Highs::changeColCost)
    //   .function("changeColBounds", &Highs::changeColBounds)
    //   .function("changeRowBounds", &Highs::changeRowBounds)
    //   .function("changeCoeff", &Highs::changeCoeff)
    //   //.function("addRows", &highs_addRows)
    //   //.function("addRow", &highs_addRow)
    //   //.function("addCol", &highs_addCol)
    //   //.function("addCols", &highs_addCols)
    //   //.function("addVar", &highs_addVar)
    //   //.function("addVars", &highs_addVars)
    //   //.function("changeColsCost", &highs_changeColsCost)
    //   //.function("changeColsBounds", &highs_changeColsBounds)
    //   //.function("changeColsIntegrality", &highs_changeColsIntegrality)
    //   //.function("deleteCols", &highs_deleteCols)
    //   //.function("deleteVars", &highs_deleteCols)  // alias
    //   //.function("deleteRows", &highs_deleteRows)
    //   //.function("setSolution", &highs_setSolution)
    //   //.function("setSolution", &highs_setSparseSolution)
    //   //.function("setBasis", &highs_setBasis)
    //   //.function("setBasis", &highs_setLogicalBasis)
    //   .function("modelStatusToString", &Highs::modelStatusToString)
    //   .function("solutionStatusToString", &Highs::solutionStatusToString)
    //   .function("basisStatusToString", &Highs::basisStatusToString)
    //   .function("basisValidityToString", &Highs::basisValidityToString)
    //   //.function("setCallback", &highs_setCallback)
    //   .function("startCallback",
    //        static_cast<HighsStatus (Highs::*)(const HighsCallbackType)>(
    //            &Highs::startCallback))
    //   .function("stopCallback",
    //        static_cast<HighsStatus (Highs::*)(const HighsCallbackType)>(
    //            &Highs::stopCallback))
    //   .function("startCallbackInt", static_cast<HighsStatus (Highs::*)(const int)>(
    //                                &Highs::startCallback))
    //   .function("stopCallbackInt", static_cast<HighsStatus (Highs::*)(const int)>(
    //                               &Highs::stopCallback));
        ;
    enum_<HighsStatus>("HighsStatus")
        .value("kError",HighsStatus::kError)
        .value("kOk",HighsStatus::kOk)
        .value("kWarning",HighsStatus::kWarning);
}