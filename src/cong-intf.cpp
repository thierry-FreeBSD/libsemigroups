//
// libsemigroups - C++ library for semigroups and monoids
// Copyright (C) 2019 James D. Mitchell
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "cong-intf.hpp"

#include "constants.hpp"                // for UNDEFINED
#include "froidure-pin-base.hpp"        // for FroidurePinBase
#include "libsemigroups-debug.hpp"      // for LIBSEMIGROUPS_ASSERT
#include "libsemigroups-exception.hpp"  // for LIBSEMIGROUPS_EXCEPTION
#include "report.hpp"                   // for REPORT_VERBOSE_DEFAULT
#include "stl.hpp"                      // for detail::to_string

namespace libsemigroups {
  ////////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - constructors + destructor - public
  ////////////////////////////////////////////////////////////////////////////

  CongruenceInterface::CongruenceInterface(congruence_type type)
      : Runner(),
        // Non-mutable
        _gen_pairs(),
        _nr_gens(UNDEFINED),
        _parent(nullptr),
        _type(type),
        // Mutable
        _init_ntc_done(),
        _is_obviously_finite(false),
        _is_obviously_infinite(false),
        _quotient(nullptr),
        _non_trivial_classes() {
    reset();
  }

  ////////////////////////////////////////////////////////////////////////////
  // Runner - pure virtual overridden function - public
  ////////////////////////////////////////////////////////////////////////////

  void CongruenceInterface::run() {
    if (nr_generators() == UNDEFINED) {
      LIBSEMIGROUPS_EXCEPTION("no generators have been set!");
    } else if (finished()) {
      // TODO(later) this shouldn't be in here but in Runner itself
      return;
    } else if (stopped()) {
      // TODO(later) this shouldn't be in here but in Runner itself
      clear_stoppage();
    }
    run_impl();
  }

  ////////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - non-pure virtual methods - public
  ////////////////////////////////////////////////////////////////////////////

  tril CongruenceInterface::const_contains(word_type const& u,
                                           word_type const& v) const {
    validate_word(u);
    validate_word(v);
    if (u == v) {
      return tril::TRUE;
    }
    class_index_type uu, vv;
    try {
      uu = const_word_to_class_index(u);
      vv = const_word_to_class_index(v);
    } catch (LibsemigroupsException const& e) {
      REPORT_VERBOSE_DEFAULT("ignoring exception:\n%s", e.what());
      return tril::unknown;
    }
    if (uu == UNDEFINED || vv == UNDEFINED) {
      return tril::unknown;
    } else if (uu == vv) {
      return tril::TRUE;
    } else if (finished()) {
      return tril::FALSE;
    } else {
      return tril::unknown;
    }
  }

  void CongruenceInterface::set_nr_generators(size_t n) {
    if (nr_generators() != UNDEFINED) {
      if (nr_generators() != n) {
        LIBSEMIGROUPS_EXCEPTION("cannot change the number of generators");
      } else {
        return;  // do nothing
      }
    } else if (n == 0) {
      LIBSEMIGROUPS_EXCEPTION("the number of generators must be non-zero!");
    } else if (started()) {
      LIBSEMIGROUPS_EXCEPTION(
          "cannot set the number of generator at this stage");
    }
    _nr_gens = n;
    set_nr_generators_impl(n);
    reset();
  }

  /////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - non-virtual methods - public
  /////////////////////////////////////////////////////////////////////////

  void CongruenceInterface::add_pair(word_type const& u, word_type const& v) {
    if (started()) {
      LIBSEMIGROUPS_EXCEPTION(
          "cannot add further generating pairs at this stage");
    }
    validate_word(u);
    validate_word(v);
    if (u == v) {
      return;
    } else if (has_parent_froidure_pin()
               && parent_froidure_pin()->equal_to(u, v)) {
      return;
    }
    // Note that _gen_pairs might contain pairs of distinct words that
    // represent the same element of the parent semigroup (if any).
    _gen_pairs.emplace_back(u, v);
    add_pair_impl(u, v);
    reset();
  }

  word_type CongruenceInterface::class_index_to_word(class_index_type i) {
    if (nr_generators() == UNDEFINED) {
      LIBSEMIGROUPS_EXCEPTION("no generators have been defined");
    } else if (i >= nr_classes()) {
      LIBSEMIGROUPS_EXCEPTION("invalid class index, expected a value in the "
                              "range [0, %d), found %d",
                              nr_classes(),
                              i);
    }
    return class_index_to_word_impl(i);
  }

  // Basic exception guarantee (since is_quotient_obviously_infinite() may
  // change the object).
  std::shared_ptr<FroidurePinBase>
  CongruenceInterface::quotient_froidure_pin() {
    if (_quotient != nullptr) {
      LIBSEMIGROUPS_ASSERT(kind() == congruence_type::twosided);
      return _quotient;
    } else if (kind() != congruence_type::twosided) {
      LIBSEMIGROUPS_EXCEPTION("the congruence must be two-sided");
    } else if (is_quotient_obviously_infinite()) {
      LIBSEMIGROUPS_EXCEPTION(
          "cannot find the quotient semigroup, it is infinite");
    }
    _quotient = quotient_impl();
    _quotient->immutable(true);
    return _quotient;
  }

  bool CongruenceInterface::is_quotient_obviously_infinite() {
    // If has_parent_froidure_pin(), then that is either finite (and so this
    // is not obviously infinite), or infinite, which is undecidable in
    // general, so we leave the answer to this question to
    // is_quotient_obviously_infinite_impl in the derived class.
    if (nr_generators() == UNDEFINED) {
      // If nr_generators() is undefined, then there is no quotient yet,
      // and so it is not obviously infinite, or anything!
      REPORT_VERBOSE("not obviously infinite (no generators yet defined)");
      return false;
    } else if (has_quotient_froidure_pin()
               && quotient_froidure_pin()->finished()) {
      // If the quotient FroidurePin is fully enumerated, it must be
      // finite, and hence this is not (obviously) infinite.
      REPORT_VERBOSE("not obviously infinite (finite)");
      return false;
    } else if (has_parent_froidure_pin() && parent_froidure_pin()->finished()) {
      REPORT_VERBOSE("not obviously infinite (parent finite)");
      return false;
    } else if (is_quotient_obviously_infinite_impl()) {
      // The derived class of CongruenceInterface knows the quotient is
      // infinite
      return true;
    }
    REPORT_VERBOSE("the quotient is not obviously infinite . . .");
    return false;
  }

  bool CongruenceInterface::is_quotient_obviously_finite() {
    if ((has_quotient_froidure_pin() && quotient_froidure_pin()->finished())
        || (has_parent_froidure_pin() && parent_froidure_pin()->finished())
        || is_quotient_obviously_finite_impl()) {
      return true;
    }
    return false;
  }

  size_t CongruenceInterface::nr_classes() {
    if (nr_generators() == UNDEFINED) {
      return UNDEFINED;
    } else if (!finished() && is_quotient_obviously_infinite()) {
      return POSITIVE_INFINITY;
    }
    return nr_classes_impl();
  }

  CongruenceInterface::class_index_type
  CongruenceInterface::word_to_class_index(word_type const& word) {
    // validate_word throws if nr_generators is undefined.
    validate_word(word);
    return word_to_class_index_impl(word);
  }

  /////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - non-virtual methods - protected
  /////////////////////////////////////////////////////////////////////////

  void CongruenceInterface::set_parent_froidure_pin(
      std::shared_ptr<FroidurePinBase> prnt) noexcept {
    LIBSEMIGROUPS_ASSERT(_parent == nullptr);
    LIBSEMIGROUPS_ASSERT(nr_generators() == UNDEFINED
                         || prnt->nr_generators() == nr_generators());
    LIBSEMIGROUPS_ASSERT(!started());
    LIBSEMIGROUPS_ASSERT(!finished());
    if (nr_generators() == UNDEFINED) {
      set_nr_generators(prnt->nr_generators());
    }
    _parent = prnt;
    reset();
  }

  /////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - non-pure virtual methods - private
  /////////////////////////////////////////////////////////////////////////

  void CongruenceInterface::add_pair_impl(word_type const&, word_type const&) {}

  CongruenceInterface::class_index_type
  CongruenceInterface::const_word_to_class_index(word_type const&) const {
    return UNDEFINED;
  }

  void CongruenceInterface::set_nr_generators_impl(size_t) {
    // do nothing
  }

  std::shared_ptr<CongruenceInterface::non_trivial_classes_type>
  CongruenceInterface::non_trivial_classes_impl() {
    if (_parent == nullptr) {
      // This means this was constructed from an fp semigroup that did not
      // have an isomorphic FroidurePin instance computed at the time. Since
      // we don't currently store the fp semigroup, we cannot ask for the
      // isomorphic FroidurePin now.
      //
      // TODO(later) store the FpSemigroup used to create this and here try
      // to compute the isomorphic FroidurePin.
      LIBSEMIGROUPS_EXCEPTION("there's no parent semigroup in which to "
                              "find the non-trivial classes");
    }
    auto ntc = non_trivial_classes_type(nr_classes(), std::vector<word_type>());

    word_type w;
    for (size_t pos = 0; pos < _parent->size(); ++pos) {
      _parent->factorisation(w, pos);
      LIBSEMIGROUPS_ASSERT(word_to_class_index(w) < ntc.size());
      ntc[word_to_class_index(w)].push_back(w);
    }
    ntc.erase(std::remove_if(ntc.begin(),
                             ntc.end(),
                             [](std::vector<word_type> const& klass) -> bool {
                               return klass.size() <= 1;
                             }),
              ntc.end());
    return std::make_shared<non_trivial_classes_type>(ntc);
  }

  /////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - non-virtual methods - private
  /////////////////////////////////////////////////////////////////////////

  void CongruenceInterface::init_non_trivial_classes() {
    if (!_init_ntc_done) {
      _non_trivial_classes = non_trivial_classes_impl();
      _init_ntc_done       = true;
    }
  }

  void CongruenceInterface::reset() noexcept {
    set_finished(false);
    _non_trivial_classes.reset();
    _init_ntc_done = false;
    _quotient.reset();
    _is_obviously_finite   = false;
    _is_obviously_infinite = false;
  }

  /////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - non-virtual methods - protected
  /////////////////////////////////////////////////////////////////////////

  bool CongruenceInterface::validate_letter(letter_type c) const {
    if (nr_generators() == UNDEFINED) {
      LIBSEMIGROUPS_EXCEPTION("no generators have been defined");
    }
    return c < _nr_gens;
  }

  void CongruenceInterface::validate_word(word_type const& w) const {
    for (auto l : w) {
      // validate_letter throws if no generators are defined
      if (!validate_letter(l)) {
        LIBSEMIGROUPS_EXCEPTION(
            "letter index out of bounds in word %s, expected "
            "value in [0, %d), got %d",
            detail::to_string(w),
            l,
            _nr_gens);
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - non-virtual static methods - protected
  /////////////////////////////////////////////////////////////////////////

  std::string const&
  CongruenceInterface::congruence_type_to_string(congruence_type typ) {
    switch (typ) {
      case congruence_type::twosided:
        return STRING_TWOSIDED;
      case congruence_type::left:
        return STRING_LEFT;
      case congruence_type::right:
        return STRING_RIGHT;
      default:
        LIBSEMIGROUPS_EXCEPTION("incorrect type");
    }
  }

  /////////////////////////////////////////////////////////////////////////
  // CongruenceInterface - static data members - private
  /////////////////////////////////////////////////////////////////////////

  const std::string CongruenceInterface::STRING_TWOSIDED = "two-sided";
  const std::string CongruenceInterface::STRING_LEFT     = "left";
  const std::string CongruenceInterface::STRING_RIGHT    = "right";
}  // namespace libsemigroups
