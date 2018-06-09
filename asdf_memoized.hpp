#ifndef ASDF_MEMOIZED_HPP
#define ASDF_MEMOIZED_HPP

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace ASDF {

using namespace std;

template <typename T> class memoized_state {
  function<shared_ptr<T>()> fun;
  bool have_value;
  shared_ptr<T> value;

public:
  memoized_state() = delete;
  memoized_state(function<shared_ptr<T>()> fun1)
      : fun(move(fun1)), have_value(false) {}

  bool cached() const { return have_value; }
  void fill_cache() {
    if (have_value)
      return;
    value = fun();
    have_value = true;
  }
  void forget() {
    if (!have_value)
      return;
    value.reset();
    have_value = false;
  }

  shared_ptr<T> get() {
    fill_cache();
    return value;
  }
};

template <typename T> class memoized {
  shared_ptr<memoized_state<T>> state;

public:
  memoized() = default;
  memoized(const memoized &) = default;
  memoized(memoized &&) = default;
  memoized &operator=(const memoized &) = default;
  memoized &operator=(memoized &&) = default;

  memoized(function<shared_ptr<T>()> fun1)
      : state(make_shared<memoized_state<T>>(move(fun1))) {}

  bool valid() const { return bool(state); }
  void reset() { state.reset(); }

  bool cached() const { return state->cached(); }
  void fill_cache() const { state->fill_cache(); }
  void forget() const { state->forget(); }

  shared_ptr<T> get() const { return state->get(); }

  const T &operator*() const { return *get(); }
  T &operator*() { return *get(); }
  const T *operator->() const { return get().get(); }
  T *operator->() { return get(); }
};

// // Modelled after std::make_shared
// template <typename T, class... Args>
// memoized<T> make_memoized(const Args &... args) {
//   return memoized<T>(
//       [=]() { return make_shared<T>(forward<Args...>(args...)); });
// }

// Modelled after std::experimental::make_ready_future
template <typename T>
memoized<typename decay<T>::type> make_fixed_memoized(T &&arg) {
  typedef typename decay<T>::type R;
  auto parg = make_shared<R>(forward<T>(arg));
  return memoized<R>([=]() { return parg; });
}

// Modelled after std::experimental::make_ready_future
template <typename T>
memoized<T> make_constant_memoized(const shared_ptr<T> &arg) {
  return memoized<T>([=]() { return arg; });
}

} // namespace ASDF

#endif // #ifndef ASDF_MEMOIZED_HPP
