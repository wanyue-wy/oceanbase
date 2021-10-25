/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef _OB_EXPR_FUNC_SEQ_LASTVAL_H
#define _OB_EXPR_FUNC_SEQ_LASTVAL_H
#include "sql/engine/expr/ob_expr_operator.h"
#include "share/ob_i_sql_expression.h"

namespace oceanbase {
namespace sql {
class ObPhysicalPlanCtx;
class ObExprFuncSeqLastval : public ObFuncExprOperator {
public:
  explicit ObExprFuncSeqLastval(common::ObIAllocator& alloc);
  virtual ~ObExprFuncSeqLastval();
  int calc_result_type2(ObExprResType& type, ObExprResType& type1, ObExprResType& type2, common::ObExprTypeCtx& type_ctx) const;
  int calc_result2(common::ObObj& result, const common::ObObj& obj1, const common::ObObj& obj2, common::ObExprCtx& expr_ctx) const;
  int cg_expr(ObExprCGCtx& expr_cg_ctx, const ObRawExpr& raw_expr, ObExpr& rt_expr) const;
  static int calc_sequence_lastval(const ObExpr& expr, ObEvalCtx& ctx, ObDatum& result);

private:
  // disallow copy
  DISALLOW_COPY_AND_ASSIGN(ObExprFuncSeqLastval);
};
}  // end namespace sql
}  // end namespace oceanbase
#endif
