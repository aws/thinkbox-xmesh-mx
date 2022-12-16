// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
namespace boost {

template <class T>
struct range_mutable_iterator<Tab<T>> {
    typedef T* type;
};

template <class T>
struct range_const_iterator<Tab<T>> {
    typedef const T* type;
};

} // namespace boost

template <class T>
inline T* range_begin( Tab<T>& tab ) {
    if( tab.Count() > 0 ) {
        return tab.Addr( 0 );
    } else {
        return 0;
    }
}

template <class T>
inline const T* range_begin( const Tab<T>& tab ) {
    if( tab.Count() > 0 ) {
        return tab.Addr( 0 );
    } else {
        return 0;
    }
}

template <class T>
inline T* range_end( Tab<T>& tab ) {
    if( tab.Count() > 0 ) {
        return tab.Addr( 0 ) + tab.Count();
    } else {
        return 0;
    }
}

template <class T>
inline const T* range_end( const Tab<T>& tab ) {
    if( tab.Count() > 0 ) {
        return tab.Addr( 0 ) + tab.Count();
    } else {
        return 0;
    }
}
