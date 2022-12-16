// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
template <class T>
class const_value_simulated_object_accessor {
    T m_value;

  public:
    const_value_simulated_object_accessor() {}

    const_value_simulated_object_accessor( T val )
        : m_value( val ) {}

    const T& at_time( TimeValue /*t*/ ) { return m_value; }

    std::size_t size() { return 1; }
};
