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

#define USING_LOG_PREFIX SQL_RESV

#include "sql/resolver/dcl/ob_alter_user_profile_resolver.h"

#include "share/schema/ob_schema_struct.h"
#include "sql/session/ob_sql_session_info.h"
#include "sql/resolver/dcl/ob_grant_resolver.h"
#include "share/ob_rpc_struct.h"
using namespace oceanbase::sql;
using namespace oceanbase::common;
using oceanbase::share::schema::ObUserInfo;

ObAlterUserProfileResolver::ObAlterUserProfileResolver(ObResolverParams& params) : ObDCLResolver(params)
{}

ObAlterUserProfileResolver::~ObAlterUserProfileResolver()
{}

int ObAlterUserProfileResolver::resolve_set_role(const ParseNode& parse_tree)
{
  int ret = OB_SUCCESS;
  ObAlterUserProfileStmt* stmt = NULL;
  uint64_t tenant_id = OB_INVALID_ID;

  if (OB_ISNULL(params_.session_info_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("not init", K(ret));
  } else if (OB_ISNULL(params_.schema_checker_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("not init", K(ret));
  } else if (T_SET_ROLE != parse_tree.type_ || 1 != parse_tree.num_child_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("wrong root", K(ret), K(parse_tree.type_), K(parse_tree.num_child_));
  } else if (OB_ISNULL(stmt = create_stmt<ObAlterUserProfileStmt>())) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("Failed to create ObAlterUserProfileStmt", K(ret));
  } else {
    ObString user_name;
    ObString host_name(OB_DEFAULT_HOST_NAME);
    const ObUserInfo* user_info = NULL;
    if (OB_FAIL(params_.schema_checker_->get_user_info(params_.session_info_->get_user_id(), user_info))) {
      LOG_WARN("get user info failed", K(ret));
    } else if (NULL == user_info) {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("current user info is null", K(ret));
    } else {

      obrpc::ObAlterUserProfileArg& arg = stmt->get_ddl_arg();
      arg.tenant_id_ = params_.session_info_->get_effective_tenant_id();
      stmt->set_set_role_flag(1);

      /* 1. resolve default role */
      OZ(resolve_default_role_clause(parse_tree.children_[0], arg, user_info->get_role_id_array(), false));
    }
  }
  return ret;
}

int ObAlterUserProfileResolver::resolve_role_list(const ParseNode* role_list, obrpc::ObAlterUserProfileArg& arg,
    const ObIArray<uint64_t>& role_id_array, bool for_default_role_stmt)
{
  int ret = OB_SUCCESS;
  CK(OB_NOT_NULL(role_list));
  CK(OB_NOT_NULL(params_.session_info_));
  CK(OB_NOT_NULL(params_.schema_checker_));
  if (OB_SUCC(ret)) {
    for (int i = 0; OB_SUCC(ret) && i < role_list->num_child_; ++i) {
      uint64_t role_id = OB_INVALID_ID;
      ParseNode* role = role_list->children_[i];
      if (NULL == role) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("role opt identified by node is null", K(ret));
      } else if (T_IDENT == role->type_) {
      } else if (T_SET_ROLE_PASSWORD != role->type_) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("role type is error", K(ret), K(role->type_));
      } else if (1 != role->value_) {
        ret = OB_NOT_SUPPORTED;
        LOG_WARN("not supported", K(ret), K(role->value_));
      } else if (NULL == role->children_[0]) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("role node is null", K(ret));
      } else {
        role = role->children_[0];
      }
      if (OB_SUCC(ret)) {
        ObString role_name;
        const ObUserInfo* role_info = NULL;
        role_name.assign_ptr(const_cast<char*>(role->str_value_), static_cast<int32_t>(role->str_len_));
        ObString host_name(OB_DEFAULT_HOST_NAME);
        if (OB_FAIL(params_.schema_checker_->get_user_info(arg.tenant_id_, role_name, host_name, role_info))) {
          if (OB_USER_NOT_EXIST == ret || OB_ISNULL(role_info) || !role_info->is_role()) {
            if (for_default_role_stmt) {
              ret = OB_ROLE_NOT_EXIST;
              LOG_USER_ERROR(OB_ROLE_NOT_EXIST, role_name.length(), role_name.ptr());
              LOG_WARN("role not exists", K(ret), K(role_name));
            } else {
              ret = OB_ERR_ROLE_NOT_GRANTED_OR_DOES_NOT_EXIST;
              LOG_USER_ERROR(OB_ERR_ROLE_NOT_GRANTED_OR_DOES_NOT_EXIST, role_name.length(), role_name.ptr());
              LOG_WARN("role not granted or does not exists", K(ret), K(role_name));
            }
          }
          LOG_WARN("fail to get user id", K(ret), K(role_name), K(host_name));
        } else if (role_info == NULL) {
          if (for_default_role_stmt) {
            ret = OB_ROLE_NOT_EXIST;
            LOG_USER_ERROR(OB_ROLE_NOT_EXIST, role_name.length(), role_name.ptr());
            LOG_WARN("role not exists", K(ret), K(role_name));
          } else {
            ret = OB_ERR_ROLE_NOT_GRANTED_OR_DOES_NOT_EXIST;
            LOG_USER_ERROR(OB_ERR_ROLE_NOT_GRANTED_OR_DOES_NOT_EXIST, role_name.length(), role_name.ptr());
            LOG_WARN("role not granted or does not exists", K(ret), K(role_name));
          }
        } else {
          role_id = role_info->get_user_id();
          if (is_ora_public_role(role_id)) {
            ret = OB_ERR_MISSING_OR_INVALID_ROLE_NAME;
          } else if (has_exist_in_array(arg.role_id_array_, role_id)) {
            /* if role duplicate in role list, then raise error */
            ret = OB_PRIV_DUP;
          } else if (!has_exist_in_array(role_id_array, role_id)) {
            /* if role not granted to user, then raise error */
            if (for_default_role_stmt) {
              ret = OB_ERR_DEFAULT_ROLE_NOT_GRANTED_TO_USER;
              LOG_USER_ERROR(OB_ERR_DEFAULT_ROLE_NOT_GRANTED_TO_USER, role_name.length(), role_name.ptr());
              LOG_WARN("role not granted to user", K(ret), K(role_name));
            } else {
              ret = OB_ERR_ROLE_NOT_GRANTED_OR_DOES_NOT_EXIST;
              LOG_USER_ERROR(OB_ERR_ROLE_NOT_GRANTED_OR_DOES_NOT_EXIST, role_name.length(), role_name.ptr());
              LOG_WARN("role not granted or does not exists", K(ret), K(role_name));
            }
          } else {
            OZ(arg.role_id_array_.push_back(role_id));
          }
        }
      }
    }
  }
  return ret;
}

int ObAlterUserProfileResolver::resolve_default_role_clause(const ParseNode* parse_tree,
    obrpc::ObAlterUserProfileArg& arg, const ObIArray<uint64_t>& role_id_array, bool for_default_role_stmt)
{
  int ret = OB_SUCCESS;
  CK(OB_NOT_NULL(parse_tree));
  if (T_DEFAULT_ROLE != parse_tree->type_ || (1 != parse_tree->num_child_ && 2 != parse_tree->num_child_)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("wrong root", K(ret), K(parse_tree->type_), K(parse_tree->num_child_));
  } else {
    if (1 == parse_tree->num_child_) {
      CK(OB_NOT_NULL(parse_tree->children_[0]));
      if (OB_SUCC(ret)) {
        if (1 == parse_tree->children_[0]->value_) {
          OX(arg.default_role_flag_ = obrpc::OB_DEFAULT_ROLE_ALL);
        } else {
          CK(3 == parse_tree->children_[0]->value_);
          OX(arg.default_role_flag_ = obrpc::OB_DEFAULT_ROLE_NONE);
        }
      }
    } else {
      CK(2 == parse_tree->num_child_);
      if (OB_SUCC(ret)) {
        if (0 == parse_tree->children_[0]->value_) {
          OX(arg.default_role_flag_ = obrpc::OB_DEFAULT_ROLE_LIST);
        } else {
          CK(2 == parse_tree->children_[0]->value_);
          OX(arg.default_role_flag_ = obrpc::OB_DEFAULT_ROLE_ALL_EXCEPT);
        }
        OZ(resolve_role_list(parse_tree->children_[1], arg, role_id_array, for_default_role_stmt));
      }
    }
  }

  return ret;
}

int ObAlterUserProfileResolver::resolve_default_role(const ParseNode& parse_tree)
{
  int ret = OB_SUCCESS;
  ObAlterUserProfileStmt* stmt = NULL;
  uint64_t tenant_id = OB_INVALID_ID;

  if (OB_ISNULL(params_.session_info_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("not init", K(ret));
  } else if (OB_ISNULL(params_.schema_checker_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("not init", K(ret));
  } else if (T_ALTER_USER_DEFAULT_ROLE != parse_tree.type_ || 2 != parse_tree.num_child_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("wrong root", K(ret), K(parse_tree.type_), K(parse_tree.num_child_));
  } else if (OB_ISNULL(stmt = create_stmt<ObAlterUserProfileStmt>())) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("Failed to create ObAlterUserProfileStmt", K(ret));
  } else {
    ObString user_name;
    ObString host_name;
    const ObUserInfo* user_info = NULL;
    obrpc::ObAlterUserProfileArg& arg = stmt->get_ddl_arg();

    /* 1. resolve user */
    tenant_id = params_.session_info_->get_effective_tenant_id();
    arg.tenant_id_ = tenant_id;
    ParseNode* user_with_host_name = parse_tree.children_[0];
    // Get user_name and host_name
    if (OB_ISNULL(user_with_host_name)) {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("user_with_host_name is NULL");
    } else {
      ParseNode* user_name_node = user_with_host_name->children_[0];
      ParseNode* host_name_node = user_with_host_name->children_[1];
      if (OB_ISNULL(user_name_node)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("user_name is NULL", K(ret), K(user_name));
      } else {
        user_name = ObString(user_name_node->str_len_, user_name_node->str_value_);
      }
      if (NULL != host_name_node) {
        host_name = ObString(host_name_node->str_len_, host_name_node->str_value_);
      } else {
        host_name = ObString(OB_DEFAULT_HOST_NAME);
      }
    }
    OZ(params_.schema_checker_->get_user_info(tenant_id, user_name, host_name, user_info),
        tenant_id,
        user_name,
        host_name);
    if (ret == OB_USER_NOT_EXIST) {
      LOG_USER_ERROR(OB_USER_NOT_EXIST, user_name.length(), user_name.ptr());
    }
    if (OB_SUCC(ret)) {
      if (user_info == NULL) {
        ret = OB_USER_NOT_EXIST;
        LOG_USER_ERROR(OB_USER_NOT_EXIST, user_name.length(), user_name.ptr());
      } else {
        arg.user_id_ = user_info->get_user_id();
      }
    }

    /* 2. resolve default role */
    OZ(resolve_default_role_clause(parse_tree.children_[1], arg, user_info->get_role_id_array(), true));
  }
  return ret;
}

int ObAlterUserProfileResolver::resolve(const ParseNode& parse_tree)
{
  int ret = OB_SUCCESS;
  ParseNode* user_with_host_name = NULL;
  ParseNode* user_name = NULL;
  ParseNode* host_name = NULL;
  ParseNode* user_profile = NULL;
  ParseNode* profile_name = NULL;
  ObAlterUserProfileStmt* lock_alter_user_profile_stmt = NULL;

  if (OB_ISNULL(params_.session_info_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("not init", K(ret));
  } else if (T_SET_ROLE == parse_tree.type_) {
    OZ(resolve_set_role(parse_tree));
  } else if (T_ALTER_USER_DEFAULT_ROLE == parse_tree.type_) {
    OZ(resolve_default_role(parse_tree));
  } else if (T_ALTER_USER_PROFILE != parse_tree.type_ || 2 != parse_tree.num_child_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("wrong root", K(ret), K(parse_tree.type_), K(parse_tree.num_child_));
  } else if (OB_ISNULL(user_with_host_name = parse_tree.children_[0]) ||
             OB_ISNULL(user_profile = parse_tree.children_[1])) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("child is null", K(parse_tree.children_[0]), K(parse_tree.children_[1]), K(ret));
  } else if (T_USER_WITH_HOST_NAME != user_with_host_name->type_ || user_with_host_name->num_child_ != 2) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("1st child type error",
        "type",
        user_with_host_name->type_,
        "child_num",
        user_with_host_name->num_child_,
        K(ret));
  } else if (T_USER_PROFILE != user_profile->type_ || user_profile->num_child_ != 1) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("2nd child type error", "type", user_profile->type_, "child_num", user_profile->num_child_, K(ret));
  } else if (OB_ISNULL(user_name = user_with_host_name->children_[0])) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("child is null", K(ret));
  } else if (OB_ISNULL(profile_name = user_profile->children_[0])) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("child is null", K(ret));
  } else if (OB_ISNULL(lock_alter_user_profile_stmt = create_stmt<ObAlterUserProfileStmt>())) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("Failed to create ObAlterUserProfileStmt", K(ret));
  } else {
    host_name = user_with_host_name->children_[1];
    obrpc::ObAlterUserProfileArg& arg = lock_alter_user_profile_stmt->get_ddl_arg();
    arg.default_role_flag_ = OB_INVALID_ID;
    arg.user_name_ = ObString(user_name->str_len_, user_name->str_value_);
    if (NULL != host_name) {
      arg.host_name_ = ObString(host_name->str_len_, host_name->str_value_);
    } else {
      arg.host_name_ = ObString(OB_DEFAULT_HOST_NAME);
    }
    if (profile_name->type_ != T_DEFAULT) {
      arg.profile_name_ = ObString(profile_name->str_len_, profile_name->str_value_);
    }
    arg.tenant_id_ = params_.session_info_->get_effective_tenant_id();
  }

  if (OB_SUCC(ret) && T_SET_ROLE != parse_tree.type_ && ObSchemaChecker::is_ora_priv_check()) {
    OZ(schema_checker_->check_ora_ddl_priv(session_info_->get_effective_tenant_id(),
           session_info_->get_priv_user_id(),
           ObString(""),
           stmt::T_ALTER_USER_PROFILE,
           session_info_->get_enable_role_array()),
        session_info_->get_effective_tenant_id(),
        session_info_->get_user_id());
  }

  return ret;
}
