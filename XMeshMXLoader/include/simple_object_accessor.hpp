// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

class simple_object_accessor_base {
  private:
    Animatable* m_owner;

  protected:
    ParamID m_paramID;
    IParamBlock2* m_pblock;

    inline virtual ~simple_object_accessor_base() {}

    // These are defined in scripted_object_ref_impl.hpp
    inline simple_object_accessor_base( Animatable* owner, const ParamID paramID )
        : m_owner( owner )
        , m_paramID( paramID ) {}
    inline void validate() {
        if( m_owner ) {
            m_pblock = m_owner->GetParamBlock( 0 );
            if( !m_pblock ) {
                throw std::runtime_error( "simple_object_accessor_base Error: Param block 0 is NULL" );
            }
            if( m_paramID >= m_pblock->NumParams() ) {
                throw std::runtime_error( "simple_object_accessor_base Error: ParamID out of range" );
            }
        } else {
            m_pblock = 0;
            throw std::runtime_error( "simple_object_accessor_base Error: owner is NULL" );
        }
    }
};

template <class T>
class simple_object_accessor : public simple_object_accessor_base {
  private:
    class helper;
    class tab_helper;

    T get_tab_value( TimeValue t, int index );
    void set_tab_value( TimeValue t, int index, const T& val );

    simple_object_accessor();

  public:
    simple_object_accessor( Animatable* owner, const ParamID paramID );

    std::size_t size();

    helper at_time( TimeValue t );

    tab_helper operator[]( int index );
};

template <class T>
simple_object_accessor<T>::simple_object_accessor()
    : simple_object_accessor_base( NULL, "<invalid>" ) {
    // Error -- you have an unbound accessor !
}

template <class T>
simple_object_accessor<T>::simple_object_accessor( Animatable* owner, const ParamID paramID )
    : simple_object_accessor_base( owner, paramID ) {}

template <class T>
T simple_object_accessor<T>::get_tab_value( TimeValue t, int index ) {
    validate();

    T result = T();
    Interval ivl;
    m_pblock->GetValue( m_paramID, t, result, ivl, index );

    return result;
}

/**
 * Internal method used by helper and tab_helper to actually set the paramblock value
 * @param  t      The time at which to set the value
 * @param  index  The tab index to set the value for
 * @param  val    The value to set into the paramblock
 */
template <class T>
void simple_object_accessor<T>::set_tab_value( TimeValue t, int index, const T& val ) {
    validate();
    m_pblock->SetValue( m_paramID, t, val, index );
}

/**
 * Template specialization for strings
 */
template <>
inline frantic::tstring simple_object_accessor<frantic::tstring>::get_tab_value( TimeValue t, int index ) {
    validate();

#if MAX_VERSION_MAJOR < 12
    TCHAR* val = 0;
#else
    const TCHAR* val = 0;
#endif
    Interval ivl;
    m_pblock->GetValue( m_paramID, t, val, ivl, index );

    if( val != 0 )
        return frantic::tstring( val );
    else
        return _T("");
}

/**
 * Template specialization for strings
 */
template <>
inline void simple_object_accessor<frantic::tstring>::set_tab_value( TimeValue t, int index,
                                                                     const frantic::tstring& val ) {
    validate();
    m_pblock->SetValue( m_paramID, t, const_cast<TCHAR*>( val.c_str() ), index );
}

/**
 * Template specialization for bool
 */
template <>
inline bool simple_object_accessor<bool>::get_tab_value( TimeValue t, int index ) {
    validate();

    BOOL val = 0;
    Interval ivl;
    m_pblock->GetValue( m_paramID, t, val, ivl, index );
    return val != 0;
}

/**
 * Template specialization for bool
 */
template <>
inline void simple_object_accessor<bool>::set_tab_value( TimeValue t, int index, const bool& b ) {
    validate();

    BOOL val( b );
    m_pblock->SetValue( m_paramID, t, val, index );
}

/**
 * Helper class of simple_object_accessor. Is returned to the user from scripted_object_ref::at_time()
 * and allows a single assignment or value access. I'm 99% sure this has no overhead and is optimized away.
 * <p>
 * Allows for such syntax as: "m_sizeParam.at_time(t) = 100.f;" or "float size = m_sizeParam.at_time(t);"
 */
template <class T>
class simple_object_accessor<T>::helper {
  private:
    friend class simple_object_accessor<T>;
    friend class simple_object_accessor<T>::tab_helper;

    simple_object_accessor& m_owner;
    TimeValue m_time;
    int m_index;

    helper( simple_object_accessor& owner, TimeValue t, int index )
        : m_owner( owner )
        , m_time( t )
        , m_index( index ) {}
    helper( const helper& rhs );
    helper& operator=( const helper& rhs );

  public:
    operator const T() const { return m_owner.get_tab_value( m_time, m_index ); }
    T& operator=( const T& rhs ) {
        m_owner.set_tab_value( m_time, m_index, rhs );
        return const_cast<T&>( rhs );
    }
};

template <class T>
class simple_object_accessor<T>::tab_helper {
  private:
    friend class simple_object_accessor<T>;
    template <class U>
    friend class simple_object_accessor;

    simple_object_accessor& m_owner;
    int m_index;

    tab_helper( simple_object_accessor& owner, int index )
        : m_owner( owner )
        , m_index( index ) {}
    tab_helper( const tab_helper& rhs );
    tab_helper& operator=( const tab_helper& rhs );

  public:
    helper at_time( TimeValue t ) { return helper( m_owner, t, m_index ); }
    const T at_time( TimeValue t ) const { return m_owner.get_tab_value( t, m_index ); }
};

template <class T>
std::size_t simple_object_accessor<T>::size() {
    validate();
    return m_pblock->Count( m_paramID );
}

template <class T>
typename simple_object_accessor<T>::helper simple_object_accessor<T>::at_time( TimeValue t ) {
    return helper( *this, t, 0 );
}

template <class T>
typename simple_object_accessor<T>::tab_helper simple_object_accessor<T>::operator[]( int index ) {
    return tab_helper( *this, (int)index );
}
