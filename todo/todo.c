/*
 * Syntax that we cannot parse yet, but would like to.
 */

// WINAPI stuff

typedef HRESULT WINAPI (*ddraw_create_proc_t)(
  GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);

// TurboC memory models
int dpmi_set_pm_int_vect(uint8_t int_no, void far *isr);

// TurboC interrupt && memory model
typedef void (interrupt far *pm_int_fn_t)(void);

// TurboC x86 inline assembler
int main(void)
{
        _asm {
                mov ah, biosfn
                int 16h
                pushf
                pop flags
                mov cc, al
                mov sc, ah
        }

	return 0;
}
