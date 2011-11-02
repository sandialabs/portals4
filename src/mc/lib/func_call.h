#ifndef FUNC_CALL_H
#define FUNC_CALL_H

#define FUNC_CALL( cq, func, ... ) if_##func( cq, __VA_ARGS__ )

#endif
