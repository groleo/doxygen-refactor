What
-----
Project aimed at refactoring Doxygen to use STL instead of qtools.

Why
---
Current qtools/ is a bit outdated so it's missing a few nice functionalities (const iterators).
Upgrading qtools/ is not that easy and it would not fix the problem of deprecation in future.
So I thought I'd use STL for its stable API. 

Refactorings status
-------------------

- [ ] QDict <T> -> std::unordered_map<std::string, T*>
  - [x] variable declaration QDictIterator
  - [ ] QDictIterator<T> li(children) -> std::list<T*>::iterator li = children.begin()
  - [x] field declaration QList
- [ ] QList <T> -> std::list<T*>
  - [x] class inheriting QList
  - [x] variable declaration QList
  - [x] field declaration QList
  - [x] parameter declaration QList
  - [x] getFirst() -> std::list::front()
  - [x] getLast() -> std::list::end()
  - [x] isEmpty() -> std::list::empty()
  - [x] count() -> std::list::size()
  - [ ] QList->setAutoDelete(TRUE) -> unique_ptr
    - [ ] BUG: setAutoDelete called in template classes is not matched
    - [x] append(x) -> std::list::push_back(std::make_unique(x))
    - [x] prepend(x) -> std::list::push_front(std::make_unique(x))
  - [ ] return ref: QList<T> & cxxMethodDecl()
  - [ ] return ptr: QList<T> * cxxMethodDecl()
  - [ ] return obj: QList<T>   cxxMethodDecl()
  - [x] return ref: QList<T> & functionDecl()
  - [x] return ptr: QList<T> * functionDecl()
  - [x] return obj: QList<T>   functionDecl()
  - [x] new expression: new QList<T>
  - [x] QList<T> constructor
  - [ ] remove(item) -> ?
  - [ ] remove(index) -> ?
  - [ ] findRef(item) -> ?
- [ ] QListIterator <T> -> std::list<T*>::iterator
  - [x] class inheriting QListIterator
  - [x] variable declaration QListIterator
  - [ ] QListIterator<T> li(children) -> std::list<T*>::iterator li = children.begin()
  - [x] return ref: QListIterator<T> & cxxMethodDecl()
  - [x] return ptr: QListIterator<T> * cxxMethodDecl()
  - [x] return obj: QListIterator<T>   cxxMethodDecl()
  - [ ] return ref: QListIterator<T> & functionDecl()
  - [ ] return ptr: QListIterator<T> * functionDecl()
  - [ ] return obj: QListIterator<T>   functionDecl()
- [ ] QIntDict <T> -> std::map<T*>
  - [ ] QIntDict<T> -> std::unordered_map<long, T*>
  - [ ] constructor QIntDict<T> (N) -> std::unordered_map<T*>::reserve(N)
  - [ ] QIntDictIterator(9)
  - [ ] classes inheriting QIntDict(4)
  - [ ] QIntDict::setAutoDelete(TRUE) -> unique_ptr
- [ ] QDict
  - [ ] QDict<T> -> std::unordered_map<std::string, T*>
  - [ ] QDictIterator<T> -> std::unordered_map<std::string, T*>::iterator
  - [ ] constructor QDict<T>(N) -> std::unordered_map<>::reserve(N)
  - [ ] QDict<T>::resize(N) -> std::unordered_map<T*>::reserve(N)
- [ ] QSDict
- [ ] QStack
- [ ] QArray
- [ ] QMap
- [ ] QStringList
- [ ] QVector
- [ ] QCache
  - [ ] QCacheIterator
 also catch hasType(pointsTo(namedDecl ...
