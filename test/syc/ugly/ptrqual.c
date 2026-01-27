/*
 * Comparing and assigning differently qualified versions of pointers.
 */

int *ip;			// ^int
const int *cip;			// ^const int
const int *const cicp;		// const^const int

void foo(void)
{
	(void)(ip == cip);	// ^int == ^const int			OK
	ip = cip;		// ^int <- ^const int 			DISC
	(void)(ip == cicp);	// ^int == const^const int		OK
	ip = cicp;		// ^int <- const^const int		DISC

	(void)(cip == ip);	// ^const int == ^int			OK
	cip = ip;		// ^const int <- ^int			OK
	(void)(cip == cicp);	// ^const int == const^const int	OK
	cip = cicp;		// ^const int <- const^const int	OK
}

int **ipp;			// ^^int
const int **cipp;		// ^^const int
int *const *ipcp;		// ^const^int
const int *const *cipcp;	// ^const^const int

void bar(void)
{
	(void)(ipp == cipp);	// ^^int == ^^const int			INC
	ipp = cipp;		// ^^int <- ^^const int			INC
	(void)(ipp == ipcp);	// ^^int == ^^const int			OK
	ipp = ipcp;		// ^^int <- ^const^int			DISC
	(void)(ipp == cipcp);	// ^^int == ^const^const int		INC
	ipp = cipcp;		// ^^int <- ^const^const int		INC

	(void)(cipp == ipp);	// ^^const in == ^^int			INC
	cipp = ipp;		// ^^const int <- ^^int			INC
	(void)(cipp == ipcp);	// ^^const int == ^const^int		INC
	cipp = ipcp;		// ^^const int <- ^const^int		INC
	(void)(cipp == cipcp);	// ^^const int == ^const^const int	OK
	cipp = cipcp;		// ^^const int <- ^const^const int	DISC

	(void)(ipcp == ipp);	// ^const^int == ^^int			OK
	ipcp = ipp;		// ^const^int <- ^^int			OK
	(void)(ipcp == cipp);	// ^const^int == ^^const int		INC
	ipcp = cipp;		// ^const^int <- ^^const int		INC
	(void)(ipcp == cipcp);	// ^const^int == ^const^const int	INC
	ipcp = cipcp;		// ^const^int <- ^const^const int	INC

	(void)(cipcp == ipp);	// ^const^const int == ^^int		INC
	cipcp = ipp;		// ^const^const int <- ^^int		INC
	(void)(cipcp == cipp);	// ^const^const int == ^^const int	OK
	cipcp = cipp;		// ^const^const int <- ^^const int	OK
	(void)(cipcp == ipcp);	// ^const^const int == ^const^int	INC
	cipcp = ipcp;		// ^const^const int <- ^const^int	INC
}
