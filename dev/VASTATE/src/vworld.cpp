

/*
 * Virtual world model defination
 * vworld.cpp
 *
 * ver 0.1 (2008/04/23)
 */

#include "precompile.h"
#include "vworld.h"

// VValueFactory Function implementation 
///////////////////////////////////////

namespace VASTATE {

    template<> 
    std::vector<uchar_t> VSimpleValue<VValue::TS_STRING, std::string>::encodeToVec () const
    {
        vector<uchar_t> out;
        const uchar_t *ch = (uchar_t *) _value.c_str ();

        for (size_t s = 0; s < _value.size (); ++ s)
            out.push_back (ch[s]);

        return out;
    }

    template<> 
    bool VSimpleValue<VValue::TS_STRING, std::string>::decodeFromVec (const std::vector<uchar_t>& vec)
    {
        _value.clear ();

        vector<uchar_t>::const_iterator it = vec.begin ();
        for (; it != vec.end (); it ++)
            _value = _value + (char) (*it);

        return true;
    }

    VValue* VValueFactory::copy (const VValue& value)
    {
        using namespace std;

        VContainer *newc = NULL;

        switch (value.get_type ())
        {
        case VValue::TS_BOOL:
            return new VSimpleValue_bool (*(VSimpleValue_bool*) & value);

        case VValue::TS_INT:
            return new VSimpleValue_int  (*(VSimpleValue_int*) & value);

        case VValue::TS_DOUBLE:
            return new VSimpleValue_double (*(VSimpleValue_double*) & value);

        case VValue::TS_STRING:
            return new VSimpleValue_string (*(VSimpleValue_string*) & value);

        case VValue::T_CONTAINER:
            return new VContainer (* (VContainer *) & value);

        case VValue::T_OBJECT:
            return new VObject (* (VObject *) & value);

        case VValue::T_VEOBJECT:
            return new VEObject (* (VEObject *) & value);
        
            /*
        case VValue::T_VEOBJECT:
            {
                newc = new VEObject (
            }

        case VValue::T_OBJECT:
            {
                VObject * oldo = (VObject) &value;
                if (newc == NULL)
                    newc = new VObject (oldo->get_id ());
            }

        case VValue::T_CONTAINER:
            {
                VContainer *oldc = (VContainer *) & value;
                VContainer *newc = new VContainer ();
                VContainer::Box::iterator it = oldc->_b.begin ();
                for (; it != oldc->_b.end (); it ++)
                    newc->add_attribute (it->first, *it->second);
                return newc;
            }*/

        default:
            cout << "VValueFactory: copy (): Copy unknown type(" << (int) value.get_type () << ") of VValue" << endl;
        }

        return NULL;
    }

    static size_t _vecToCharAr (const vector<uchar_t>& v, uchar_t *ar, size_t maxlen)
    {
        // buffer is too small to put anything
        if (maxlen < sizeof(size_t))
            return -1;

        uchar_t *arcur = ar + sizeof(size_t);
        size_t len = v.size ();
        memcpy (ar, (char *) &len, sizeof(size_t));

        vector<uchar_t>::const_iterator it = v.begin ();
        for (; it != v.end () && (size_t)(arcur-ar) <= maxlen; it ++)
        {
            *arcur = *it;
            arcur ++;
        }

        // if buffer is full before all chars filled in
        if (it != v.end () && (size_t)(arcur-ar) == maxlen)
            return 0;

        return (size_t)(arcur - ar);
    }

    static int _charArToVec (vector<uchar_t> & v, const uchar_t *ar, /*i/o parameter*/ size_t &arlen)
    {
        if (arlen < sizeof(size_t))
            return -1;

        size_t s = *(size_t*)(ar);
        // total size must less or equal decoding string
        if (arlen < s + sizeof(size_t))
            return -1;

        s += sizeof(size_t);
        for (size_t si = sizeof(size_t); si < s; ++ si)
            v.push_back (ar[si]);

        arlen = s;
        return 0;
    }

#define __DO(x) do { x; } while (0);
#define __encode_enqueue_string(str,strmax,data,datasize) __DO(\
    if ((str) + (datasize) > (strmax))   \
        return -1;                       \
    memcpy ((str), (data), (datasize));  \
    (str) += (datasize);                 )
#define __encode_enqueue_data(d,dlen,v)             \
    for (int __i = 0; __i < (dlen); __i ++)         \
        (v).push_back(((unsigned char *)(d))[__i]);
#define __encode_enqueue_dataitem(d,dlen,v) __DO(   \
    std::vector<unsigned char> __t;                 \
    __encode_enqueue_data(d,dlen,__t);              \
    v.push_back(__t);                               )

    int VValueFactory::encode (const VValue *v, uchar_t * outbuffer, /* i/o parameber */ size_t& out_len)
    {
        if (outbuffer == NULL)
            return -1;

        /*
        char *b = _encodebuffer;
        int bl = 0;     // used length of b
        const char *bmax = _encodebuffer + _encodebufferlen;

        char *d = _encodebuffer2;
        int dl = 0;     // used length of d
        const char dmax = _encodebuffer2 + _encodebufferlen;

        char *o = outbuffer;
        int ol = 0;     // used length of o
        const char *omax = outbuffer + out_len;
        */
        index_t ind = 0;
        std::vector<std::vector<uchar_t> > dbuf;
        std::vector<uchar_t> ov;

        std::stack<const VValue *> s;
        s.push (v);

#ifdef DEBUG_ENCODING_STDOUT
        cout << "begin encoding ========" << endl;
#endif
        while (!s.empty ())
        {      
            const VValue *d = s.top (); s.pop ();

            objecttype_t type = d->get_type ();
            __encode_enqueue_data (&type, sizeof(objecttype_t), ov);
            //__encode_enqueue_string(o, omax, &type, sizeof(objecttype_t));
#ifdef DEBUG_ENCODING_STDOUT
            cout << (int) d->get_type () << " ";
#endif
            switch (d->get_type ())
            {
            case VValue::T_VEOBJECT:
                {
                    const VEObject *dv = (VEObject *) d;
                    //__encode_enqueue_string (b, bmax, &(dv->pos), sizeof(Coord3D));
                    __encode_enqueue_dataitem (&(dv->pos), sizeof(Coord3D), dbuf);
                    //__encode_enqueue_string (o, omax, &ind, sizeof(unsigned short)); ind ++;
                    ind = (index_t) dbuf.size () - 1;
                    __encode_enqueue_data (&ind, sizeof(index_t), ov);
#ifdef DEBUG_ENCODING_STDOUT
                    cout << "ind_pos ";
#endif
                }

            case VValue::T_OBJECT:
                {
                    const VObject *dv = (VObject *) d;
                    const id_t tid = dv->get_id ();
                    //__encode_enqueue_string (_encodebuffer, bl, blen, &(tid), sizeof(id_t));
                    __encode_enqueue_dataitem (&tid, sizeof(id_t), dbuf);
                    //__encode_enqueue_string (outbuffer, ol, out_len, &ind, sizeof(unsigned short)); ind ++;
                    ind = (index_t) dbuf.size () - 1;
                    __encode_enqueue_data (&ind, sizeof(index_t), ov);
#ifdef DEBUG_ENCODING_STDOUT
                    cout << "ind_id ";
#endif
                }
            case VValue::T_CONTAINER:
                {
                    const VContainer *dc = (VContainer *) d;
                    uchar_t osize = (uchar_t) dc->_b.size ();
                    //__encode_enqueue_string (outbuffer, ol, out_len, &osize, sizeof (unsigned char));
                    __encode_enqueue_data (&osize, sizeof(unsigned char), ov);
#ifdef DEBUG_ENCODING_STDOUT
                    cout << dc->_b.size () << " ";
#endif
                    //VContainer::Box::const_reverse_iterator it = dc->_b.rbegin ();
                    std::map<VASTATE::short_index_t,VValue *>::const_iterator itf = dc->_b.begin ();
                    for (; itf != dc->_b.end (); itf ++)
                    {
                        short_index_t idx = itf->first;
                        //__encode_enqueue_string (outbuffer, ol, out_len, &ind, sizeof (short_index_t)); ind ++;
                        __encode_enqueue_data (&idx, sizeof(short_index_t), ov);
#ifdef DEBUG_ENCODING_STDOUT
                        cout << (int) itf->first << " ";
#endif
                    }

                    std::map<VASTATE::short_index_t,VValue *>::const_reverse_iterator it = dc->_b.rbegin ();
                    it = dc->_b.rbegin ();
                    for (; it != dc->_b.rend (); it ++)
                        s.push (it->second);

                    break;
                }
            case VValue::TS_BOOL:
            case VValue::TS_INT:
            case VValue::TS_DOUBLE:
            case VValue::TS_STRING:
                {
                    // push data
                    //__encode_enqueue_string (outbuffer, ol, out_len, &ind, sizeof (unsigned short)); ind ++;
                    dbuf.push_back (d->encodeToVec ());
                    ind = (index_t) dbuf.size () - 1;
                    __encode_enqueue_data (&ind, sizeof(index_t), ov);
                }
                break;

            default:
                cerr << "VValueFactory::encode (): Unknown value type (" << (int) d->get_type () << ") of VValue." << endl;
            }
        }
#ifdef DEBUG_ENCODING_STDOUT
        cout << "\nend encoding ==========" << endl;
#endif


        // code ov and dbuf into a single char array
        size_t result = _vecToCharAr (ov, outbuffer, out_len);
        if (result == 0)
            return -1;

        /*
        result += sizeof(size_t);
        if (result > out_len)
            return -1;
        */

        * (size_t *)(outbuffer+result) = dbuf.size ();
        result += sizeof(size_t);
        for (std::vector<std::vector<unsigned char> >::iterator itd = dbuf.begin (); itd != dbuf.end (); itd ++)
        {
            size_t result_n = _vecToCharAr (*itd, outbuffer+result, out_len-result);
            cout << "r = " << result_n << endl;
            result += result_n;
            if (result > out_len)
                return -1;
        }

        out_len = result;

        // debug info output
        cout << "bin out =================" << endl;
        {
            std::vector<unsigned char>::iterator it = ov.begin ();
            for (; it != ov.end (); it ++)
                cout << (int) (*it) << " ";
        }
        cout << endl << "data ====================" << endl;
        {
            int count = 0;
            std::vector<std::vector<unsigned char> >::iterator it = dbuf.begin ();
            for (; it != dbuf.end (); it ++)
            {
                cout << count++ << ": ";
                std::vector<unsigned char> & v = *it;
                if (v.size () == 0)
                {
                    cout << "(empty)" << endl;
                    continue;
                }

                std::vector<unsigned char>::iterator it2 = v.begin ();
                for (; it2 != v.end (); it2 ++)
                    cout << (int) (*it2) << " ";
                cout << endl;
            }
            
        }
        cout << "data encoded ========================" << endl;
        for (int j = 0; j < out_len; ++j)
            cout << (int) outbuffer[j] << " ";
        cout << endl;
        cout << "=========================" << endl;
        //////////////////////

        return 0;
    }

    //VValue * VValueFactory::decode (const vector<unsigned char> & stru, const vector<vector<unsigned char> > & data)
    VValue * VValueFactory::decode (const unsigned char *buffer, const size_t & buffer_len)
    {
        // structures for in-function usage
        struct _cache {
            objecttype_t type;
            Coord3D pos;
            id_t id;
        };
        _cache cache;

        // result object
        VValue *result = NULL;
        // stack for differenting layer of objects
        stack<int> count_a;
        stack<short_index_t> idx_a;
        stack<VValue *> ref_a;

        vector<unsigned char> stru;
        vector<vector<unsigned char> > data;

        // decode char array
        size_t buffer_ran = 0;
        size_t buffer_len_tmp = buffer_len;
        // decoding object structure array
        if (_charArToVec (stru, buffer, buffer_len_tmp) == -1)
            return NULL;

        // decoding data array
        // the first size_t is count of data item
        size_t dbuflen = *(size_t *)(buffer+buffer_len_tmp);
        buffer_ran = buffer_len_tmp + 4;
        // then decode dbuflen times for data item
        for (size_t ddbuf = 0; ddbuf < dbuflen; ++ ddbuf)
        {
            vector<unsigned char> dataitem;
            buffer_len_tmp = buffer_len - buffer_ran;
            if (_charArToVec (dataitem, buffer+buffer_ran, buffer_len_tmp) == -1)
                return NULL;
            buffer_ran += buffer_len_tmp;
            data.push_back (dataitem);
        }

        ///////////////////////////////////////////////////////////////
        /*
        cout << "bin out =================" << endl;
        {
            std::vector<unsigned char>::iterator it = stru.begin ();
            for (; it != stru.end (); it ++)
                cout << (int) (*it) << " ";
        }
        cout << endl << "data ====================" << endl;
        {
            int count = 0;
            std::vector<std::vector<unsigned char> >::iterator it = data.begin ();
            for (; it != data.end (); it ++)
            {
                cout << count++ << ": ";
                std::vector<unsigned char> & v = *it;
                if (v.size () == 0)
                {
                    cout << "(empty)" << endl;
                    continue;
                }

                std::vector<unsigned char>::iterator it2 = v.begin ();
                for (; it2 != v.end (); it2 ++)
                    cout << (int) (*it2) << " ";
                cout << endl;
            }
            
        }
        */
        /////////////////////////////////////////////////////////////////////

        // decode objects        
        vector<unsigned char>::const_iterator stru_iter = stru.begin ();
        for (; stru_iter != stru.end ();)
        {
            objecttype_t type = *stru_iter ++;

            // per-round objects
            VValue *t_obj = NULL;
            short_index_t t_idx = 0;

            if (!idx_a.empty ())
            {
                t_idx = idx_a.top ();
                idx_a.pop ();
            }

            // reset caching variable
            memset (&cache, 0, sizeof(cache));
            cache.type = VValue::T_UNKNOWN;
            
            if (!count_a.empty ())
                count_a.top () --;

            switch (type)
            {
                case VValue::T_VEOBJECT:
                {
                    cache.type = VValue::T_VEOBJECT;
                    index_t ref = (*(stru_iter++)) + ((*(stru_iter++)) >> 8);

                    // decode pos
                    {
                        uchar_t * ch = (uchar_t *) & (cache.pos);
                        for (vector<uchar_t>::iterator it = data[ref].begin (); it != data[ref].end (); it ++)
                            *ch ++ = *it;
                    }
                    /////////////

                    // debugging info
                    cout << "pos: ref(" << (int) ref << ")" << endl;
                }

                case VValue::T_OBJECT:
                {
                    if (cache.type == VValue::T_UNKNOWN)
                        cache.type = VValue::T_OBJECT;
                    index_t ref = (*(stru_iter++)) + ((*(stru_iter++)) >> 8);
                    //cache.id = refer(ref);
                    cache.id = (id_t) ref;

                    // debugging info
                    cout << "id: ref(" << (int) ref << ")" << endl;
                }

                case VValue::T_CONTAINER:
                {
                    if (cache.type == VValue::T_UNKNOWN)
                        cache.type = VValue::T_CONTAINER;

                    switch (cache.type)
                    {
                        case VValue::T_VEOBJECT: t_obj = new VEObject (cache.id, cache.pos); break;
                        case VValue::T_OBJECT:   t_obj = new VObject  (cache.id);            break;
                        case VValue::T_CONTAINER:t_obj = new VContainer ();                  break;
                    }

                    // debugging info
                    cout << "level: " << count_a.size () << " ";
                    cout << "inds: ";

                    count_a.push (*(stru_iter++));

                    // reversing index sequence
                    stack<short_index_t> temp_st;
                    for (int i = 0; i < count_a.top (); i ++)
                    {
                        short_index_t idx = *(stru_iter++);
                        temp_st.push (idx);

                        cout << (int)idx << " ";
                    }
                    cout << endl;

                    for (; !temp_st.empty (); temp_st.pop ())
                        idx_a.push (temp_st.top ());
                }
                break;

                case VValue::TS_BOOL:
                case VValue::TS_INT:
                case VValue::TS_DOUBLE:
                case VValue::TS_STRING:
                {
                    index_t ref = (*(stru_iter++)) + ((*(stru_iter++)) >> 8);
                    switch (type)
                    {
                        case VValue::TS_BOOL:   t_obj = new VSimpleValue_bool ();   break;
                        case VValue::TS_INT:    t_obj = new VSimpleValue_int ();    break;
                        case VValue::TS_DOUBLE: t_obj = new VSimpleValue_double (); break;
                        case VValue::TS_STRING: t_obj = new VSimpleValue_string (); break;
                    }
                    if (ref >= data.size ())
                        cerr << "VValueFactory::decode (): Invalid data reference (" << (int) ref << ") ." << endl;
                    else
                        t_obj->decodeFromVec (data[ref]);

                    // debugging info
                    cout << "level: " << count_a.size () << " ";
                    cout << "value: type(" << (int) type << ") ref(" << (int) ref << ")" << endl;
                }
                break;

                default:
                    cerr << "VValueFactory::decode (): Unknown value type (" << (int) type << ") of VValue." << endl;
            }

            if (ref_a.empty ())
                result = t_obj;
            else
            {
                VContainer *c = (VContainer*) ref_a.top ();
                c->_b[t_idx] = t_obj;
            }

            if (type == VValue::T_VEOBJECT || type == VValue::T_OBJECT || type == VValue::T_CONTAINER)
                ref_a.push (t_obj);
                
            while (count_a.size () != 0 && count_a.top () == 0)
            {
                count_a.pop ();
                ref_a.pop ();
            }
        }

        return result;
    }

    void VValueFactory::destroy (const VValue *m)
    {
        delete m;
    }

} /* namespace VASTATE */
