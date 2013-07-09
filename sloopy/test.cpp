template<typename T>
class B {
public:
  int i;
  void f() { }
};

template<typename T>
class D : public B<T> {
    using B<T>::i;
    using B<T>::f;
public:
  void g()
  {
    i++;
    f();
  }
};

