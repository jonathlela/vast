

#ifndef _ERROROUTPUT_H
#define _ERROROUTPUT_H

#include "config.h"

class EXPORT errout
{
public:
	errout ();
	virtual ~errout ();
	void setout (errout * out);
	void output (const char * str);
	virtual void outputHappened (const char * str);
private:
	static errout * _actout;
};


#endif /* _ERROROUTPUT_H */

