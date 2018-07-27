SRC_DIR=$(dir $(MAKEFILE_LIST))

OBJS= \
ast_node.o \
ast_to_ir.o \
ast_type.o \
branch_predication.o \
bset.o \
c95.tab.o \
cg_bb.o \
cg_cond.o \
cg_data.o \
cg_dom.o \
cg_func.o \
cg_import.o \
cg_instr.o \
cg_print.o \
driver.o \
dset.o \
emit.o \
graph.o \
graph_loop.o \
ir_bb.o \
ir_data.o \
ir_dom.o \
ir_func.o \
ir_node.o \
ir_print.o \
ir_sim.o \
ir_type.o \
ir_validate.o \
iselect.o \
lex.cg_yy.o \
lex.yy.o \
mem.o \
mem2reg.o \
regalloc_ssa.o \
symbol.o

CC=gcc
CFLAGS=-O0 -g3 -Wall -Werror `pkg-config --cflags glib-2.0`
LIBS=`pkg-config --libs glib-2.0`

VPATH=$(SRC_DIR):$(SRC_DIR)/frontend:$(SRC_DIR)/ir:$(SRC_DIR)/test:$(SRC_DIR)/ir_passes:$(SRC_DIR)/cg:$(SRC_DIR)/util

driver : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

c95.tab.c c95.tab.h : c95.y
	bison -d $<

lex.yy.c : c95.l
	flex $<

lex.cg_yy.c : cg_lexer.l
	flex $<

%.o : %.c c95.tab.h
	$(CC) $(CFLAGS) -c $< -I$(SRC_DIR) -I.

clean :
	rm -f driver $(OBJS) c95.tab.c  c95.tab.h  lex.yy.c lex.cg_yy.c
