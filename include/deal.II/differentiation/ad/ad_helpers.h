// ---------------------------------------------------------------------
//
// Copyright (C) 2016 - 2017 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE at
// the top level of the deal.II distribution.
//
// ---------------------------------------------------------------------

#ifndef dealii_differentiation_ad_ad_helpers_h
#define dealii_differentiation_ad_ad_helpers_h

#include <deal.II/base/config.h>

#if defined(DEAL_II_WITH_ADOLC) || defined(DEAL_II_TRILINOS_WITH_SACADO)

#  include <deal.II/base/numbers.h>

#  include <deal.II/differentiation/ad/ad_drivers.h>
#  include <deal.II/differentiation/ad/ad_number_traits.h>
#  include <deal.II/differentiation/ad/adolc_number_types.h>
#  include <deal.II/differentiation/ad/adolc_product_types.h>
#  include <deal.II/differentiation/ad/sacado_number_types.h>
#  include <deal.II/differentiation/ad/sacado_product_types.h>

#  include <algorithm>
#  include <iostream>
#  include <iterator>
#  include <numeric>
#  include <set>

DEAL_II_NAMESPACE_OPEN

namespace Differentiation
{
  namespace AD
  {
    /**
     * A base helper class that facilitates the evaluation of the derivative(s)
     * of a number of user-defined dependent variables $\mathbf{f}(\mathbf{X})$
     * with respect to a set of independent variables $\mathbf{X}$, that is
     * $\dfrac{d^{i} \mathbf{f}(\mathbf{X})}{d \mathbf{X}^{i}}$.
     *
     * In addition to the dimension @p dim, this class is templated on the
     * floating point type @p scalar_type of the number that we'd like to
     * differentiate, as well as an enumeration indicating the @p ADNumberTypeCode .
     * The @p ADNumberTypeCode dictates which auto-differentiation library is
     * to be used, and what the nature of the underlying auto-differentiable
     * number is. Refer to the @ref auto_symb_diff module for more details in
     * this regard.
     *
     * For all of the classes derived from this base class, there are two
     * possible ways that the code in which they are used can be structured.
     * The one approach is effectively a subset of the other, and which might
     * be necessary to use depends on the nature of the chosen
     * auto-differentiable number.
     *
     * When "tapeless" numbers are employed, the most simple code structure
     * would be of the following form:
     *
     * @code
     *   // Initialize AD helper
     *   ADHelperType<dim,tapeless_AD_typecode> ad_helper (...);
     *
     *   // Register independent variables
     *   ad_helper.register_independent_variable(...);
     *
     *   // Extract the sensitive equivalent of the independent variables.
     *   // They are the auto-differentiable counterparts to the values
     *   // used as arguments to the register_independent_variable() function.
     *   // The operations conducted with these AD numbers will be tracked.
     *   const auto ad_independent_variables
     *     = ad_helper.get_sensitive_variables(...);
     *
     *   // Use the sensitive variables to compute the dependent variables.
     *   const auto ad_dependent_variables = func(sensitive_variables);
     *
     *   // Register the dependent variables with the helper class
     *   ad_helper.register_dependent_variables(ad_dependent_variables);
     *
     *   // Compute derivatives of the dependent variables
     *   const auto derivatives = ad_helper.compute_gradients();
     * @endcode
     *
     * Note that since the specialized classes interpret the independent
     * variables in different ways, above represents only an outline of the
     * steps taken to compute derivatives. More specific examples are outlined
     * in the individual classes that specialize this base class.
     *
     * When "taped" numbers are to be used, the above code should be wrapped by
     * a few more lines of code to manage the taping procedure:
     *
     * @code
     *   // Initialize AD helper
     *   ADHelperType<taped_or_tapeless_AD_typecode> ad_helper (...);
     *
     *   // An optional call to set the amount of memory to be allocated to
     *   // storing taped data
     *   ad_helper.set_tape_buffer_sizes();
     *
     *    // Select a tape number to record to
     *   const types::tape_index tape_index = ...;
     *
     *   // Indicate that we are about to start tracing the operations for
     *   // function evaluation on the tape. If this tape has already been used
     *   // (i.e. the operations are already recorded) then we (optionally) load
     *   // the tape and reuse this data.
     *   const bool is_recording
     *     = ad_helper.start_recording_operations(tape_index);
     *   if (is_recording == true)
     *   {
     *     // This is the "recording" phase of the operations.
     *     // In this block one places the majority of the operations described
     *     // in the previous code block. The set of operations that are
     *     // conducted here therefore includes the following steps:
     *     // - Register independent variables
     *     // - Extract the sensitive equivalent of the independent variables
     *     // - Use the sensitive variables to compute the dependent variables
     *     // - Register the dependent variables with the helper class
     *
     *     // Indicate that have completed tracing the operations onto the tape.
     *     ad_helper.stop_recording_operations(false); // write_tapes_to_file
     *   }
     *   else
     *   {
     *     // This is the "tape reuse" phase of the operations.
     *     // Here we will leverage the already traced operations that reside
     *     // on a tape, and simply revaluate the tape at a different point
     *     // to get the function values and their derivatives.
     *
     *     // Load the existing tape to be reused
     *     ad_helper.activate_recorded_tape(tape_no);
     *
     *     // Set the new values of the independent variables where the recorded
     *     // dependent functions are to be evaluated (and differentiated
     *     // around).
     *     ad_helper.set_independent_variable(...);
     *   }
     *
     *   // Compute derivatives of the dependent variables
     *   const auto derivatives = ad_helper.compute_gradients();
     * @endcode
     *
     * The second approach outlined here is more general than the first, and
     * will work equally well for both taped and tapeless auto-differentiable
     * numbers.
     *
     * @warning ADOL-C does not support the standard threading models used by
     * deal.II, so this class should @b not be embedded within a multithreaded
     * function when using ADOL-C number types. It is, however, suitable for use
     * in both serial and MPI routines.
     *
     * @todo Make this class thread safe for Sacado number and ADOL-C tapeless
     * numbers (if supported).
     *
     * @author Jean-Paul Pelteret, 2016, 2017, 2018
     */
    template <enum AD::NumberTypes ADNumberTypeCode,
              typename ScalarType = double>
    class ADHelperBase
    {
    public:
      /**
       * Type definition for the floating point number type that is used in,
       * and result from, all computations.
       */
      using scalar_type =
        typename AD::NumberTraits<ScalarType, ADNumberTypeCode>::scalar_type;

      /**
       * Type definition for the auto-differentiation number type that is used
       * in all computations.
       */
      using ad_type =
        typename AD::NumberTraits<ScalarType, ADNumberTypeCode>::ad_type;

      /**
       * @name Constructor / destructor
       */
      //@{

      /**
       * The constructor for the class.
       *
       * @param[in] n_independent_variables The number of independent variables
       * that will be used in the definition of the functions that it is
       * desired to compute the sensitivities of. In the computation of
       * $\mathbf{f}(\mathbf{X})$, this will be the number of inputs
       * $\mathbf{X}$, i.e. the dimension of the range space.
       * @param[in] n_dependent_variables The number of scalar functions to be
       * defined that will have a sensitivity to the given independent
       * variables. In the computation of $\mathbf{f}(\mathbf{X})$, this will
       * be the number of outputs $\mathbf{f}$, i.e. the dimension of the
       * domain or image space.
       */
      ADHelperBase(const unsigned int n_independent_variables,
                   const unsigned int n_dependent_variables);

      /**
       * Destructor
       */
      virtual ~ADHelperBase();

      //@}

      /**
       * @name Interrogation of internal information
       */
      //@{

      /**
       * Returns the number of independent variables that this object expects to
       * work with. This is the dimension of the range space.
       */
      std::size_t
      n_independent_variables() const;

      /**
       * Returns the number of dependent variables that this object expects to
       * operate on. This is the dimension of the domain or image space.
       */
      std::size_t
      n_dependent_variables() const;

      /**
       * Prints the status of all queriable data. Exactly what is printed and
       * its format depends on the @p ad_type, as is determined by the
       * @p ADNumberTypeCode template parameter.
       *
       * @param[in] stream The output stream to which the values are to be
       * written.
       */
      void
      print(std::ostream &stream) const;

      /**
       * Prints the values currently assigned to the independent variables.
       *
       * @param[in] stream The output stream to which the values are to be
       * written.
       */
      void
      print_values(std::ostream &stream) const;

      /**
       * Prints the statistics regarding the usage of the tapes.
       *
       * @param[in] tape_index The index of the tape to get the statistics of.
       * @param[out] stream The output stream to which the values are to be
       * written.
       *
       * @note This function only produces meaningful output when @p ad_type
       * is a taped auto-differentiable number.
       */
      void
      print_tape_stats(const types::tape_index tape_index,
                       std::ostream &          stream) const;

      //@}

      /**
       * @name Operations specific to tapeless mode
       */
      //@{

      /**
       * Pre-specify the number of @p independent_variables to be used in
       * tapeless mode.
       *
       * Although this function is called internally in the ADHelperBase
       * constructor, there may be occasions when ADOL-C tapeless numbers
       * (<tt>adtl::adoubles</tt>) are created before an instance of this class
       * is created. This function therefore allows one to declare at the
       * earliest possible instance how many directional derivatives will be
       * considered in tapeless mode.
       *
       * @warning With @p ensure_persistent_setting set to <tt>true</tt> when
       * the @p ad_type is an ADOL-C tapeless number, calling this function
       * leaves the set number of directional derivatives in a persistent state.
       * It will therefore not be possible to further modify the number of
       * directional derivatives to be tracked by <tt>adtl::adoubles</tt>'s
       * during course of the program's execution.
       */
      static void
      configure_tapeless_mode(const unsigned int n_independent_variables,
                              const bool ensure_persistent_setting = true);

      //@}

      /**
       * @name Operations specific to taped mode: Recording tapes
       */
      //@{

      /**
       * Reset the state of the helper class.
       *
       * When an instance of an ADHelperBase is stored as a class member object
       * with the intention to reuse its instance, it may be necessary to reset
       * the state of the object before use. This is because, internally, there
       * is error checking performed to ensure that the correct
       * auto-differentiable data is being tracked and used only when
       * appropriate. This function clears all member data and, therefore,
       * allows the state of all internal flags to be safely reset to their
       * initial state.
       *
       * In the rare case that the number of independent or dependent variables
       * has changed, this can also reconfigured by passing in the appropriate
       * arguments to the function.
       *
       * @note This also resets the active tape number to an invalid number, and
       * deactivates the recording mode for taped variables.
       */
      virtual void
      reset(const unsigned int n_independent_variables =
              dealii::numbers::invalid_unsigned_int,
            const unsigned int n_dependent_variables =
              dealii::numbers::invalid_unsigned_int,
            const bool clear_registered_tapes = true);

      /**
       * Returns whether or not this class is tracking calculations performed
       * with its marked independent variables.
       */
      bool
      is_recording() const;

      /**
       * Returns the tape number which is currently activated for recording or
       * reading.
       */
      types::tape_index
      active_tape() const;

      /**
       * Returns whether or not a tape number has already been used
       * or registered.
       */
      bool
      is_registered_tape(const types::tape_index tape_index) const;

      /**
       * Set the buffer sizes for the next active tape.
       *
       * This function must be called before start_recording_operations() for
       * it to have any influence on the memory allocated to the next recorded
       * tape.
       *
       * @note This function only has an effect when using ADOL-C numbers. As
       * stated by the ADOL-C manual, it may be desirable to create a file
       * ".adolcrc" in the program run directory and set the buffer size
       * therein.
       * Alternatively, this function can be used to override the settings for
       * any given tape, or can be used in the event that no ".adolcrc" file
       * exists.
       * The default value for each buffer is set at 64 megabytes, a
       * heuristically chosen value thought to be appropriate for use within
       * the context of finite element analysis when considering coupled
       * problems with multiple vector-valued fields discretised by higher
       * order shape functions, as well as complex constitutive laws.
       *
       * @param[in] obufsize ADOL-C operations buffer size
       * @param[in] lbufsize ADOL-C locations buffer size
       * @param[in] vbufsize ADOL-C value buffer size
       * @param[in] tbufsize ADOL-C Taylor buffer size
       */
      void
      set_tape_buffer_sizes(const types::tape_buffer_sizes obufsize = 67108864,
                            const types::tape_buffer_sizes lbufsize = 67108864,
                            const types::tape_buffer_sizes vbufsize = 67108864,
                            const types::tape_buffer_sizes tbufsize = 67108864);

      /**
       * Enable recording mode for a given tape. The use of this function is
       * mandatory if the auto-differentiable number is a taped type. However,
       * for the purpose of developing generic code, it can also be safely
       * called for tapeless auto-differentiable numbers.
       *
       * The operations that take place between this function call and that
       * of stop_recording_operations() are recorded to the tape and can
       * be replayed and reevaluated as necessary.
       *
       * The typical set of operations to be performed during this "recording"
       * phase (between the calls to start_recording_operations() and
       * stop_recording_operations() ) are:
       *   - Definition of some independent variables via
       *     register_independent_variable() or
       *     register_independent_variables(). These define the branch of
       *     operations tracked by the tape. If the @p keep flag is set to
       *     <tt>true</tt> then these represent precisely the point about which
       *     the function derivatives are to be computed. If the @p keep flag is
       *     set to <tt>false</tt> then these only represent dummy values, and
       *     the point at which the function derivatives are to be computed must
       *     be set by calling set_independent_variables() again.
       *   - Extraction of a set of independent variables of auto-differentiable
       *     type using get_sensitive_variables(). These are then tracked during
       *     later computations.
       *   - Defining the dependent variables via register_dependent_variable()
       *     or register_dependent_variables(). These are the functions that
       *     will be differentiated with respect to the independent variables.
       *
       * @param[in] tape_index The index of the tape to be written
       * @param[in] overwrite_tape Express whether tapes are allowed to be
       * overwritten. If <tt>true</tt> then any existing tape with a given
       * @p tape_index will be destroyed and a new tape traced over it.
       * @param[in] keep_values Determines whether the numerical values of all
       * independent variables are recorded in the tape buffer. If true, then
       * the tape can be immediately used to perform computations after
       * recording is complete.
       *
       * @note During the recording phase, no value(), gradient(), hessian(),
       * or jacobian() operations can be performed.
       *
       * @note The chosen tape index must be greater than
       * numbers::invalid_tape_index and less than numbers::max_tape_index.
       */
      bool
      start_recording_operations(const types::tape_index tape_index,
                                 const bool              overwrite_tape = false,
                                 const bool              keep_values    = true);

      /**
       * Disable recording mode for a given tape. The use of this function is
       * mandatory if the auto-differentiable number is a taped type. However,
       * for the purpose of developing generic code, it can also be safely
       * called for tapeless auto-differentiable numbers.
       *
       * @note After this function call, the tape is considered ready for use and
       * operations such as value(), gradient() or hessian() can be executed.
       *
       * @note For taped AD numbers, this operation is only valid in recording mode.
       */
      void
      stop_recording_operations(const bool write_tapes_to_file = false);

      /**
       * Select a pre-recorded tape to read from.
       *
       * @param[in] tape_index The index of the tape to be read from.
       *
       * @note The chosen tape index must be greater than
       * numbers::invalid_tape_index and less than numbers::max_tape_index.
       */
      void
      activate_recorded_tape(const types::tape_index tape_index);

      //@}

    protected:
      /**
       * @name Taping
       */
      //@{

      /**
       * Index of the tape that is currently in use. It is this tape that will
       * be recorded to or read from when performing computations using "taped"
       * auto-differentiable numbers.
       */
      types::tape_index active_tape_index;

      /**
       * A collection of tapes that have been recorded to on this process.
       *
       * It is important to keep track of this so that one doesn't accidentally
       * record over a tape (unless specifically instructed to) and that one
       * doesn't try to use a tape that doesn't exist.
       */
      static std::set<types::tape_index> registered_tapes;

      /**
       * Mark whether we're going to inform taped data structures to retain
       * the coefficients ("Taylors" in ADOL-C nomenclature) stored on the
       * tape so that they can be evaluated again at a later stage.
       */
      bool keep_values;

      /**
       * Mark whether we're currently recording a tape. Dependent on the state
       * of this flag, only a restricted set of operations are allowable.
       */
      bool is_recording_flag;

      /**
       * A flag indicating that we should preferentially use the user-defined
       * taped buffer sizes as opposed to either the default values selected
       * by the AD library (or, in the case of ADOL-C, defined in an
       * ".adolcrc" file).
       */
      bool use_stored_taped_buffer_sizes;

      /**
       * ADOL-C operations buffer size.
       */
      types::tape_buffer_sizes obufsize;

      /**
       * ADOL-C locations buffer size.
       */
      types::tape_buffer_sizes lbufsize;

      /**
       * ADOL-C value buffer size.
       */
      types::tape_buffer_sizes vbufsize;

      /**
       * ADOL-C Taylor buffer size.
       */
      types::tape_buffer_sizes tbufsize;

      /**
       * Select a tape to record to or read from.
       *
       * This function activates a tape, but depending on whether @p read_mode
       * is set, the tape is either taken as previously written to (and put
       * into read-only mode), or cleared for (re-)taping.
       *
       * @param[in] tape_index The index of the tape to be written to/read
       *            from.
       * @param[in] read_mode A flag that marks whether or not we expect to
       *            read data from a preexisting tape.
       *
       * @note The chosen tape index must be greater than
       * numbers::invalid_tape_index and less than numbers::max_tape_index.
       */
      void
      activate_tape(const types::tape_index tape_index, const bool read_mode);

      //@}

      /**
       * @name Independent variables
       */
      //@{

      /**
       * A set of independent variables $\mathbf{X}$ that differentiation will
       * be performed with respect to.
       *
       * The gradients and Hessians of dependent variables will be computed
       * at these finite values.
       */
      mutable std::vector<scalar_type> independent_variable_values;

      /**
       * A set of sensitive independent variables $\mathbf{X}$ that
       * differentiation will be performed with respect to.
       *
       * The gradients and Hessians of dependent variables will be computed
       * using these configured AD numbers. Note that only reverse-mode AD
       * requires that the sensitive independent variables be stored.
       */
      mutable std::vector<ad_type> independent_variables;

      /**
       * A list of registered independent variables that have been manipulated
       * for a given set of operations.
       */
      std::vector<bool> registered_independent_variable_values;

      /**
       * A list of registered independent variables that have been extracted and
       * their sensitivities marked.
       */
      mutable std::vector<bool> registered_marked_independent_variables;

      /**
       * Reset the boolean vector @p registered_independent_variable_values that
       * indicates which independent variables we've been manipulating for
       * the current set of operations.
       */
      void
      reset_registered_independent_variables();

      /**
       * Set the actual value of the independent variable $X_{i}$.
       *
       * @param[in] index The index in the vector of independent variables.
       * @param[in] value The value to set the index'd independent variable to.
       */
      void
      set_sensitivity_value(const unsigned int index, const scalar_type &value);

      /**
       * Initialize an independent variable $X_{i}$ such that subsequent
       * operations performed with it are tracked.
       *
       * @note Care must be taken to mark each independent variable only once.
       *
       * @note The order in which the independent variables are marked defines the
       * order of all future internal operations. They must be manipulated in
       * the same order as that in which they are first marked. If not then,
       * for example, ADOL-C won't throw an error, but rather it might complain
       * nonsensically during later computations or produce garbage results.
       *
       * @param[in] index The index in the vector of independent variables.
       * @param[out] out An auto-differentiable number that is ready for use in
       * computations. The operations that are performed with it are recorded on
       * the tape and will be considered in the computation of dependent
       * variable values.
       */
      void
      mark_independent_variable(const unsigned int index, ad_type &out) const;

      /**
       * Finalize the state of the independent variables before use.
       *
       * This step and the storage of the independent variables is done
       * separately because some derived classes may offer the capability
       * to add independent variables in a staggered manner. This function
       * is to be triggered when these values are considered finalized
       * and we can safely initialize the sensitive equivalents of those
       * values.
       */
      void
      finalize_sensitive_independent_variables() const;

      /**
       * Initialize an independent variable $X_{i}$.
       *
       * @param[out] out An auto-differentiable number that is ready for use in
       * standard computations. The operations that are performed with it are
       * not recorded on the tape, and so should only be used when not in
       * recording mode.
       * @param[in] index The index in the vector of independent variables.
       */
      void
      initialize_non_sensitive_independent_variable(const unsigned int index,
                                                    ad_type &out) const;

      /**
       * The number of independent variables that have been manipulated within a
       * set of operations.
       */
      unsigned int
      n_registered_independent_variables() const;
      //@}

      /**
       * @name Dependent variables
       */
      //@{

      /**
       * The set of dependent variables $\mathbf{f}(\mathbf{X})$ of which the
       * derivatives with respect to $\mathbf{X}$ will be computed.
       *
       * The gradients and Hessians of these dependent variables will be
       * computed at the values $\mathbf{X}$ set with the
       * set_sensitivity_value() function.
       *
       * @note These are stored as an @p ad_type so that we can use them to
       * compute function values and directional derivatives in the case that
       * tapeless numbers are used
       */
      std::vector<ad_type> dependent_variables;

      /**
       * A list of registered dependent variables.
       */
      std::vector<bool> registered_marked_dependent_variables;

      /**
       * Reset the boolean vector @p registered_marked_dependent_variables that
       * indicates which independent variables have been manipulated by the
       * current set of operations. All entries in the vector are set to the
       * value of the @p flag.
       */
      void
      reset_registered_dependent_variables(const bool flag = false);

      /**
       * The number of dependent variables that have been registered.
       */
      unsigned int
      n_registered_dependent_variables() const;

      /**
       * Register the definition of the index'th dependent variable
       * $f(\mathbf{X})$.
       *
       * @param[in] index The index of the entry in the global list of dependent
       * variables that this function belongs to.
       * @param[in] func The recorded function that defines a dependent
       * variable.
       *
       * @note Each dependent variable must only be registered once.
       */
      void
      register_dependent_variable(const unsigned int index,
                                  const ad_type &    func);
      //@}

    private:
      /**
       * A counter keeping track of the number of helpers in existence.
       *
       * This is only important information for when we use taped number types.
       * As the tapes are stored in some global register, they exist independent
       * of these helpers. However, it is assumed that when all helpers go
       * out of scope then the tapes can be written over.
       */
      static unsigned int n_helpers;

    }; // class ADHelperBase


  } // namespace AD
} // namespace Differentiation


DEAL_II_NAMESPACE_CLOSE

#endif // defined(DEAL_II_WITH_ADOLC) || defined(DEAL_II_TRILINOS_WITH_SACADO)

#endif // dealii__adolc_helpers_h
