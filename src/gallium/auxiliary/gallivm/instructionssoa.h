/**************************************************************************
 *
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#ifndef INSTRUCTIONSSOA_H
#define INSTRUCTIONSSOA_H

#include <llvm/Support/LLVMBuilder.h>

#include <vector>

namespace llvm {
   class Module;
   class Function;
   class BasicBlock;
   class Value;
}
class StorageSoa;

class InstructionsSoa
{
public:
   InstructionsSoa(llvm::Module *mod, llvm::Function *func,
                   llvm::BasicBlock *block, StorageSoa *storage);

   std::vector<llvm::Value*> arl(const std::vector<llvm::Value*> in);

   std::vector<llvm::Value*> add(const std::vector<llvm::Value*> in1,
                                 const std::vector<llvm::Value*> in2);
   std::vector<llvm::Value*> madd(const std::vector<llvm::Value*> in1,
                                  const std::vector<llvm::Value*> in2,
                                  const std::vector<llvm::Value*> in3);
   std::vector<llvm::Value*> mul(const std::vector<llvm::Value*> in1,
                                 const std::vector<llvm::Value*> in2);
   void         end();

   std::vector<llvm::Value*> extractVector(llvm::Value *vector);
private:
   const char * name(const char *prefix) const;
   llvm::Value *vectorFromVals(llvm::Value *x, llvm::Value *y,
                               llvm::Value *z, llvm::Value *w);
private:
   llvm::LLVMFoldingBuilder  m_builder;
   StorageSoa *m_storage;
private:
   mutable int  m_idx;
   mutable char m_name[32];
};


#endif