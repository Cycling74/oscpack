/*
	oscpack -- Open Sound Control (OSC) packet manipulation library
    http://www.rossbencina.com/code/oscpack

    Copyright (c) 2004-2013 Ross Bencina <rossb@audiomulch.com>

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files
	(the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge,
	publish, distribute, sublicense, and/or sell copies of the Software,
	and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
	ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
	CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
	The text above constitutes the entire oscpack license; however, 
	the oscpack developer(s) also make the following non-binding requests:

	Any person wishing to distribute modifications to the Software is
	requested to send the modifications to the original developer so that
	they can be incorporated into the canonical version. It is also 
	requested that these non-binding requests be included whenever the
	above license is reproduced.
*/
#ifndef INCLUDED_OSCPACK_OSCRECEIVEDELEMENTS_H
#define INCLUDED_OSCPACK_OSCRECEIVEDELEMENTS_H

#include <cassert>
#include <cstddef>
#include <cstring> // size_t

#include "OscTypes.h"
#include "OscException.h"


namespace osc{


class MalformedMessageException : public Exception{
public:
    MalformedMessageException( const char *w="malformed message" )
        : Exception( w ) {}
};

class MalformedBundleException : public Exception{
public:
    MalformedBundleException( const char *w="malformed bundle" )
        : Exception( w ) {}
};

class WrongArgumentTypeException : public Exception{
public:
    WrongArgumentTypeException( const char *w="wrong argument type" )
        : Exception( w ) {}
};

class MissingArgumentException : public Exception{
public:
    MissingArgumentException( const char *w="missing argument" )
        : Exception( w ) {}
};

class ExcessArgumentException : public Exception{
public:
    ExcessArgumentException( const char *w="too many arguments" )
        : Exception( w ) {}
};


class ReceivedPacket{
public:
    ReceivedPacket( const char *contents, std::size_t size )
        : contents_( contents )
        , size_( (int32)size )
    {
        // sanity check integer types declared in OscTypes.h 
        // you'll need to fix OscTypes.h if any of these asserts fail
        assert( sizeof(osc::int32) == 4 );
        assert( sizeof(osc::uint32) == 4 );
        assert( sizeof(osc::int64) == 8 );
        assert( sizeof(osc::uint64) == 8 );
    }

    bool IsMessage() const { return !IsBundle(); }
    bool IsBundle() const;

    int32 Size() const { return size_; }
    const char *Contents() const { return contents_; }

private:
    const char *contents_;
    int32 size_; // keep as int32 (not size_t) to leave structure layout intact
};


class ReceivedBundleElement{
public:
    ReceivedBundleElement( const char *sizePtr )
        : sizePtr_( sizePtr ) {}

    friend class ReceivedBundleElementIterator;

    bool IsMessage() const { return !IsBundle(); }
    bool IsBundle() const;

    int32 Size() const;
    const char *Contents() const { return sizePtr_ + 4; }

private:
    const char *sizePtr_;
};


class ReceivedBundleElementIterator{
public:
	ReceivedBundleElementIterator( const char *sizePtr )
        : value_( sizePtr ) {}

	ReceivedBundleElementIterator operator++()
	{
        Advance();
        return *this;
	}

    ReceivedBundleElementIterator operator++(int)
    {
        ReceivedBundleElementIterator old( *this );
        Advance();
        return old;
    }

	const ReceivedBundleElement& operator*() const { return value_; }

    const ReceivedBundleElement* operator->() const { return &value_; }

	friend bool operator==(const ReceivedBundleElementIterator& lhs,
            const ReceivedBundleElementIterator& rhs );

private:
	ReceivedBundleElement value_;

	void Advance() { value_.sizePtr_ = value_.Contents() + value_.Size(); }

    bool IsEqualTo( const ReceivedBundleElementIterator& rhs ) const
    {
        return value_.sizePtr_ == rhs.value_.sizePtr_;
    }
};

inline bool operator==(const ReceivedBundleElementIterator& lhs,
        const ReceivedBundleElementIterator& rhs )
{	
	return lhs.IsEqualTo( rhs );
}

inline bool operator!=(const ReceivedBundleElementIterator& lhs,
        const ReceivedBundleElementIterator& rhs )
{
	return !( lhs == rhs );
}


class ReceivedMessageArgument{
public:
	ReceivedMessageArgument( const char *typeTagPtr, const char *argumentPtr )
		: typeTagPtr_( typeTagPtr )
		, argumentPtr_( argumentPtr ) {}

    friend class ReceivedMessageArgumentIterator;
    
	char TypeTag() const { return *typeTagPtr_; }

    // the unchecked methods below don't check whether the argument actually
    // is of the specified type. they should only be used if you've already
    // checked the type tag or the associated IsType() method.

    bool IsBool() const
        { return *typeTagPtr_ == TRUE_TYPE_TAG || *typeTagPtr_ == FALSE_TYPE_TAG; }
    bool AsBool() const;
    bool AsBoolUnchecked() const;

    bool IsNil() const { return *typeTagPtr_ == NIL_TYPE_TAG; }
    bool IsInfinitum() const { return *typeTagPtr_ == INFINITUM_TYPE_TAG; }

    bool IsInt32() const { return *typeTagPtr_ == INT32_TYPE_TAG; }
    int32 AsInt32() const;
    int32 AsInt32Unchecked() const;

    bool IsFloat() const { return *typeTagPtr_ == FLOAT_TYPE_TAG; }
    float AsFloat() const;
    float AsFloatUnchecked() const;

    bool IsChar() const { return *typeTagPtr_ == CHAR_TYPE_TAG; }
    char AsChar() const;
    char AsCharUnchecked() const;

    bool IsRgbaColor() const { return *typeTagPtr_ == RGBA_COLOR_TYPE_TAG; }
    uint32 AsRgbaColor() const;
    uint32 AsRgbaColorUnchecked() const;

    bool IsMidiMessage() const { return *typeTagPtr_ == MIDI_MESSAGE_TYPE_TAG; }
    uint32 AsMidiMessage() const;
    uint32 AsMidiMessageUnchecked() const;

    bool IsInt64() const { return *typeTagPtr_ == INT64_TYPE_TAG; }
    int64 AsInt64() const;
    int64 AsInt64Unchecked() const;

    bool IsTimeTag() const { return *typeTagPtr_ == TIME_TAG_TYPE_TAG; }
    uint64 AsTimeTag() const;
    uint64 AsTimeTagUnchecked() const;

    bool IsDouble() const { return *typeTagPtr_ == DOUBLE_TYPE_TAG; }
    double AsDouble() const;
    double AsDoubleUnchecked() const;

    bool IsString() const { return *typeTagPtr_ == STRING_TYPE_TAG; }
    const char* AsString() const;
    const char* AsStringUnchecked() const { return argumentPtr_; }

    bool IsSymbol() const { return *typeTagPtr_ == SYMBOL_TYPE_TAG; }
    const char* AsSymbol() const;
    const char* AsSymbolUnchecked() const { return argumentPtr_; }

    bool IsBlob() const { return *typeTagPtr_ == BLOB_TYPE_TAG; }
    void AsBlob( const void*& data, unsigned long& size ) const;
    void AsBlobUnchecked( const void*& data, unsigned long& size ) const;
    
private:
	const char *typeTagPtr_;
	const char *argumentPtr_;
};


class ReceivedMessageArgumentIterator{
public:
	ReceivedMessageArgumentIterator( const char *typeTags, const char *arguments )
        : value_( typeTags, arguments ) {}

	ReceivedMessageArgumentIterator operator++()
	{
        Advance();
        return *this;
	}

    ReceivedMessageArgumentIterator operator++(int)
    {
        ReceivedMessageArgumentIterator old( *this );
        Advance();
        return old;
    }

	const ReceivedMessageArgument& operator*() const { return value_; }

    const ReceivedMessageArgument* operator->() const { return &value_; }

	friend bool operator==(const ReceivedMessageArgumentIterator& lhs,
            const ReceivedMessageArgumentIterator& rhs );

private:
	ReceivedMessageArgument value_;

	void Advance();

    bool IsEqualTo( const ReceivedMessageArgumentIterator& rhs ) const
    {
        return value_.typeTagPtr_ == rhs.value_.typeTagPtr_;
    }
};

inline bool operator==(const ReceivedMessageArgumentIterator& lhs,
        const ReceivedMessageArgumentIterator& rhs )
{	
	return lhs.IsEqualTo( rhs );
}

inline bool operator!=(const ReceivedMessageArgumentIterator& lhs,
        const ReceivedMessageArgumentIterator& rhs )
{	
	return !( lhs == rhs );
}


class ReceivedMessageArgumentStream{
    friend class ReceivedMessage;
    ReceivedMessageArgumentStream( const ReceivedMessageArgumentIterator& begin,
            const ReceivedMessageArgumentIterator& end )
        : p_( begin )
        , end_( end ) {}

    ReceivedMessageArgumentIterator p_, end_;
    
public:

    // end of stream
    bool Eos() const { return p_ == end_; }

    ReceivedMessageArgumentStream& operator>>( bool& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs = (*p_++).AsBool();
        return *this;
    }

    // not sure if it would be useful to stream Nil and Infinitum
    // for now it's not possible

    ReceivedMessageArgumentStream& operator>>( int32& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs = (*p_++).AsInt32();
        return *this;
    }     

    ReceivedMessageArgumentStream& operator>>( float& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs = (*p_++).AsFloat();
        return *this;
    }

    ReceivedMessageArgumentStream& operator>>( char& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs = (*p_++).AsChar();
        return *this;
    }

    ReceivedMessageArgumentStream& operator>>( RgbaColor& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs.value = (*p_++).AsRgbaColor();
        return *this;
    }

    ReceivedMessageArgumentStream& operator>>( MidiMessage& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs.value = (*p_++).AsMidiMessage();
        return *this;
    }

    ReceivedMessageArgumentStream& operator>>( int64& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs = (*p_++).AsInt64();
        return *this;
    }
    
    ReceivedMessageArgumentStream& operator>>( TimeTag& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs.value = (*p_++).AsTimeTag();
        return *this;
    }

    ReceivedMessageArgumentStream& operator>>( double& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs = (*p_++).AsDouble();
        return *this;
    }

    ReceivedMessageArgumentStream& operator>>( Blob& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        (*p_++).AsBlob( rhs.data, rhs.size );
        return *this;
    }
    
    ReceivedMessageArgumentStream& operator>>( const char*& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs = (*p_++).AsString();
        return *this;
    }
    
    ReceivedMessageArgumentStream& operator>>( Symbol& rhs )
    {
        if( Eos() )
            throw MissingArgumentException();

        rhs.value = (*p_++).AsSymbol();
        return *this;
    }

    ReceivedMessageArgumentStream& operator>>( MessageTerminator& rhs )
    {
        (void) rhs; // suppress unused parameter warning

        if( !Eos() )
            throw ExcessArgumentException();

        return *this;
    }
};


class ReceivedMessage{
    void Init( const char *bundle, unsigned long size );
public:
    explicit ReceivedMessage( const ReceivedPacket& packet );
    explicit ReceivedMessage( const ReceivedBundleElement& bundleElement );

	const char *AddressPattern() const { return addressPattern_; }

	// Support for non-standard SuperCollider integer address patterns:
	bool AddressPatternIsUInt32() const;
	uint32 AddressPatternAsUInt32() const;

	unsigned long ArgumentCount() const { return static_cast<unsigned long>(typeTagsEnd_ - typeTagsBegin_); }

    const char *TypeTags() const { return typeTagsBegin_; }


    typedef ReceivedMessageArgumentIterator const_iterator;
    
	ReceivedMessageArgumentIterator ArgumentsBegin() const
    {
        return ReceivedMessageArgumentIterator( typeTagsBegin_, arguments_ );
    }
     
	ReceivedMessageArgumentIterator ArgumentsEnd() const
    {
        return ReceivedMessageArgumentIterator( typeTagsEnd_, 0 );
    }

    ReceivedMessageArgumentStream ArgumentStream() const
    {
        return ReceivedMessageArgumentStream( ArgumentsBegin(), ArgumentsEnd() );
    }

private:
	const char *addressPattern_;
	const char *typeTagsBegin_;
	const char *typeTagsEnd_;
    const char *arguments_;
};


class ReceivedBundle{
    void Init( const char *message, unsigned long size );
public:
    explicit ReceivedBundle( const ReceivedPacket& packet );
    explicit ReceivedBundle( const ReceivedBundleElement& bundleElement );

    uint64 TimeTag() const;

    unsigned long ElementCount() const { return elementCount_; }

    typedef ReceivedBundleElementIterator const_iterator;
    
	ReceivedBundleElementIterator ElementsBegin() const
    {
        return ReceivedBundleElementIterator( timeTag_ + 8 );
    }
     
	ReceivedBundleElementIterator ElementsEnd() const
    {
        return ReceivedBundleElementIterator( end_ );
    }

private:
    const char *timeTag_;
    const char *end_;
    unsigned long elementCount_;
};


} // namespace osc


#endif /* INCLUDED_OSCPACK_OSCRECEIVEDELEMENTS_H */
