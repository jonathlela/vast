
#include "precompile.h"
#include "rawdata.h"

namespace VAST {
    bool RawData::push_array (const uchar_t * data, size_t dsize)
    {
        this->insert (end (), data, data + dsize);
        return true;
    }

    bool RawData::push_sized_array (const uchar_t * data, size_t dsize)
    {
        push_array ((uchar_t *) &dsize, sizeof(size_t));
        push_array (data, dsize);
        return true;
    }

    bool RawData::pop_array  (uchar_t * data_b, size_t dsize)
    {
        if (size () < dsize)
            return false;

        //memcpy (data_b, &(this->operator[](0)), dsize);
        memcpy (data_b, _Myfirst, dsize);

        this->erase (begin (), begin () + dsize);

        return true;
    }

    bool RawData::pop_sized_array  (uchar_t * data_b, size_t& dsize)
    {
        size_t s;
        if (!pop_array ((uchar_t *) &s, sizeof(size_t)) || s > dsize)
            return false;

        return pop_array (data_b, (dsize = s));
    }

    void RawData::pop_front (size_t size)
    {
        this->erase (begin (), begin() + size);
    }
} /* namespace VAST */
