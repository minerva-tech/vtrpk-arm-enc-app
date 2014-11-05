#ifndef IOBSERVER_H
#define IOBSERVER_H

class  IObserver 
{
public:
	virtual void progress(int val=0) = 0; //!< Sets progress bar to the \a val value. If \a val equal to zero, some progress was achieved, but nobody knows how much was done, and how much is left.
};

#endif
