
#ifndef _RAWDATABUFFER_H
#define _RAWDATABUFFER_H

//#include "precompile.h"
#include <vector>
#include "vastate_typedef.h"

namespace VAST
{

#define rawdata_p(x) ((const uchar_t *)&(x))

    class RawData : public std::vector<uchar_t>
    {
    public:
        RawData ()
        {
        }

        ~RawData ()
        {
        }

        // push an array to back of buffer
        bool push_array       (const uchar_t * data, size_t dsize);

        // push size of array and the array to back of buffer
        bool push_sized_array (const uchar_t * data, size_t dsize);

        // concat two RawData
        inline 
        void push_raw         (const RawData & rw)
        {
            if (rw.size () > 0)
                this->insert (end (), rw.begin (), rw.end ());
        }

        // fetch a data from front of buffer
        bool pop_array        (uchar_t * data_b, size_t dsize);

        // fetch a size-marking data
        bool pop_sized_array  (uchar_t * data_b, size_t& dsize);

        // pop size bytes out from front of buffer
        void pop_front (size_t size = sizeof(uchar_t));
    };

} /* namespace VAST */
#endif
