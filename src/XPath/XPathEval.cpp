/*
 * This file is part of evQueue
 * 
 * evQueue is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * evQueue is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with evQueue. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Thibault Kummer <bob@coldsource.net>
 */

#include <XPath/XPathEval.h>
#include <XPath/XPathTokens.h>
#include <XPath/XPathParser.h>
#include <XPath/XPathOperators.h>
#include <XPath/XPathFunctions.h>
#include <DOM/DOMDocument.h>
#include <DOM/DOMNamedNodeMap.h>
#include <DOM/DOMNode.h>
#include <Exception/Exception.h>

#include <memory>

using namespace std;

// Return child nodes of context node
TokenSeq *XPathEval::get_child_nodes(const string &name,const eval_context &context,TokenSeq *node_list,bool depth)
{
	for(int i=0;i<context->items.size();i++)
	{
		if(name==".")
		{
			node_list->items.push_back(new TokenNode(*context->items.at(i)));
			continue;
		}
		else if(name=="..")
		{
			DOMNode parent = ((DOMNode)(*context->items.at(i))).getParentNode();
			if(parent)
				node_list->items.push_back(new TokenNode(parent));
			continue;
		}
		
		DOMNode node = ((DOMNode)*context->items.at(i)).getFirstChild();
		while(node)
		{
			if(node.getNodeType()==DOMNode::ELEMENT_NODE)
			{
				if(depth)
				{
					TokenSeq lcontext(new TokenNode(node));
					get_child_nodes(name,&lcontext,node_list,depth);
				}
				
				if(node.getNodeName()==name || name=="*")
					node_list->items.push_back(new TokenNode(node));
			}
			node = node.getNextSibling();
		}
	}
	
	return node_list;
}

// Return attributes of context node
TokenSeq *XPathEval::get_child_attributes(const string &name,const eval_context &context,TokenSeq *node_list,bool depth)
{
	for(int i=0;i<context->items.size();i++)
	{
		DOMNode node = *context->items.at(i);
		
		if((node.getNodeType()==DOMNode::ELEMENT_NODE || node.getNodeType()==DOMNode::DOCUMENT_NODE) && depth)
		{
			DOMNode child = node.getFirstChild();
			while(child)
			{
				TokenSeq lcontext(new TokenNode(child));
				get_child_attributes(name,&lcontext,node_list,depth);
				child = child.getNextSibling();
			}
		}
		
		if(node.getNodeType()==DOMNode::ELEMENT_NODE)
		{
			DOMNamedNodeMap map = node.getAttributes();
			for(int j=0;j<map.getLength();j++)
			{
				if(name=="*" || map.item(j).getNodeName()==name)
					node_list->items.push_back(new TokenNode((DOMNode)map.item(j)));
			}
		}
	}
	
	return node_list;
}

TokenSeq *XPathEval::get_axis(const string &axis_name,const string &node_name,const eval_context &context,TokenSeq *node_list,bool depth)
{
	for(int i=0;i<context->items.size();i++)
	{
		DOMNode node = (DOMNode)(*context->items.at(i));
		
		if((node.getNodeType()==DOMNode::ELEMENT_NODE || node.getNodeType()==DOMNode::DOCUMENT_NODE) && depth)
		{
			if(node.getFirstChild())
			{
				DOMNode subnode = node.getFirstChild();
				while(subnode)
				{
					TokenSeq lcontext(new TokenNode(subnode));
					get_axis(axis_name,node_name,&lcontext,node_list,depth);
					subnode = subnode.getNextSibling();
				}
			}
		}
		
		if(axis_name=="preceding-sibling")
		{
			while(node = node.getPreviousSibling())
			{
				if(node_name=="*" || node.getNodeName()==node_name)
					node_list->items.push_back(new TokenNode(node));
			}
		}
		else if(axis_name=="following-sibling")
		{
			while(node = node.getNextSibling())
			{
				if(node_name=="*" || node.getNodeName()==node_name)
					node_list->items.push_back(new TokenNode(node));
			}
		}
		else throw Exception("XPath Eval","Unknown axis : "+axis_name);
	}
	
	return node_list;
}

// Evaluate a node filter to see if it must be keept or node
// Xpath syntax : /node[<filter expression>]
void XPathEval::filter_token_node_list(TokenSeq *list,TokenExpr *filter)
{
	int idx = 1; // XPath uses 1 based indexes
	int size = list->items.size();
	for(int i=0;i<list->items.size();i++,idx++)
	{
		TokenExpr *filter_copy = new TokenExpr(*filter);
		TokenSeq context_seq(list->items.at(i)->clone());
		eval_context context(&context_seq,idx,size);
		Token *token;
		
		try
		{
			token = evaluate_expr(filter_copy,context);
		}
		catch(Exception &e)
		{
			delete filter_copy;
			throw;
		}
		
		if(!(bool)(*token))
		{
			delete list->items.at(i);
			list->items.erase(list->items.begin()+i);
			i--;
		}
		delete token;
	}
}

// Get nth node from a node list
// Xpath syntax : /node[<integer>]
void XPathEval::get_nth_token_node_list(TokenSeq *list,int n)
{
	int list_size = list->items.size();
	// Out of range
	if(n<=0 || n>list_size)
	{
		for(int i=0;i<list_size;i++)
			delete list->items.at(i);
		list->items.clear();
		return;
	}
	
	// Remove elements before
	for(int i=0;i<n-1;i++)
	{
		delete list->items.at(0);
		list->items.erase(list->items.begin());
	}
	
	// Remove elements after
	for(int i=n;i<list_size;i++)
	{
		delete list->items.at(1);
		list->items.erase(list->items.begin()+1);
	}
}

Token *XPathEval::evaluate_func(const std::vector<Token *> &expr_tokens, int i,const eval_context &current_context,TokenSeq *left_context)
{
	// Lookup funcion name
	TokenFunc *func = (TokenFunc *)expr_tokens.at(i);
	auto it = funcs_desc.find(func->name);
	if(it==funcs_desc.end())
		throw Exception("XPath Eval","Unknown function : "+func->name+func->LogInitialPosition());
	
	Token *ret;
	vector<Token *>args;
	try
	{
		// Evaluate function parameters
		for(int j=0;j<func->args.size();)
		{
			args.push_back(evaluate_expr(func->args.at(j),current_context));
			func->args.erase(func->args.begin());
		}
		
		// Call function implementation
		ret = it->second.impl({current_context,left_context,it->second.custom_context,this},args);
		
		for(int j=0;j<args.size();j++)
			delete args.at(j);
		
		return ret;
	}
	catch(Exception &e)
	{
		for(int j=0;j<args.size();j++)
			delete args.at(j);
		throw Exception("XPath Eval",e.context+" : "+e.error+" in function "+func->name+"()"+func->LogInitialPosition());
	}
	catch(...)
	{
		for(int j=0;j<args.size();j++)
			delete args.at(j);
		throw Exception(func->name,"Unexpected exception");
	}
}

Token *XPathEval::evaluate_node(const std::vector<Token *> &expr_tokens, int i,const eval_context &context,bool depth)
{
	TokenNodeName *node_name = (TokenNodeName *)expr_tokens.at(i);
	TokenSeq *ret = new TokenSeq();
	
	try
	{
		return get_child_nodes(node_name->name,context,ret,depth);
	}
	catch(Exception &e)
	{
		delete ret;
		throw Exception("XPath Eval",e.context+" : "+e.error+" in node name "+node_name->name+node_name->LogInitialPosition());
	}
}

Token *XPathEval::evaluate_axis(const std::vector<Token *> &expr_tokens, int i,const eval_context &context,bool depth)
{
	TokenAxis *axis= (TokenAxis *)expr_tokens.at(i);
	TokenSeq *ret = new TokenSeq();
	
	try
	{
		return get_axis(axis->name,axis->node_name,context,ret,depth);
	}
	catch(Exception &e)
	{
		delete ret;
		throw Exception("XPath Eval",e.context+" : "+e.error+" in axis "+axis->name+axis->LogInitialPosition());
	}
	
	return ret;
}

Token *XPathEval::evaluate_attribute(const std::vector<Token *> &expr_tokens, int i,const eval_context &context,bool depth)
{
	TokenAttrName *attr_name = (TokenAttrName *)expr_tokens.at(i);
	TokenSeq *ret = new TokenSeq();
	
	try
	{
		return get_child_attributes(attr_name->name,context,ret,depth);
	}
	catch(Exception &e)
	{
		delete ret;
		throw Exception("XPath Eval",e.context+" : "+e.error+" in attribute "+attr_name->name+attr_name->LogInitialPosition());
	}
}

// Evaluate a fully parsed expression
Token *XPathEval::evaluate_expr(Token *token,const eval_context &context)
{
	if(token->GetType()!=EXPR)
		return token; // Nothing to do
	
	TokenExpr *expr = (TokenExpr *)token;
	
	// Compute operators
	while(true)
	{
		// Find oprator with lower priority
		int minop = 500,minop_index = -1;
		for(int i=0;i<expr->expr_tokens.size();i++)
		{
			if(expr->expr_tokens.at(i)->GetType()==OP && ops_desc.at(((TokenOP *)expr->expr_tokens.at(i))->op).prio<minop)
			{
				minop = ops_desc.at(((TokenOP *)expr->expr_tokens.at(i))->op).prio;
				minop_index = i;
			}
		}
		
		if(minop_index==-1)
			break; // No operators left
		
		Token *left = 0, *right = 0, *new_token = 0;
		
		// Get operator
		TokenOP *op = (TokenOP *)expr->expr_tokens.at(minop_index);
		
		try
		{
			// Check left and right operands
			if(minop_index-1<0)
				throw Exception("XPath Eval","Missing left operand of operator "+Token::ToString(op->op)+op->LogInitialPosition());
			
			if(minop_index+1>=expr->expr_tokens.size())
				throw Exception("XPath Eval","Missing right operand of operator "+Token::ToString(op->op)+op->LogInitialPosition());
			
			// Get operands
			left = expr->expr_tokens.at(minop_index-1);
			right = expr->expr_tokens.at(minop_index+1);
			expr->expr_tokens.erase(expr->expr_tokens.begin()+minop_index-1,expr->expr_tokens.begin()+minop_index+2);
			
			// Evaluate left operand
			left = evaluate_expr(left,context);
			
			// Lazy logical operators
			if(op->op==OR)
			{
				TokenBool right(false);
				new_token = ops_desc.at(op->op).impl(left,&right);
				if((bool)(*new_token)==false)
				{
					delete new_token;
					new_token = 0;
				}
			}
			else if(op->op==AND)
			{
				TokenBool right(true);
				new_token = ops_desc.at(op->op).impl(left,&right);
				if((bool)(*new_token)==true)
				{
					delete new_token;
					new_token = 0;
				}
			}
			
			if(new_token==0)
			{
				//  Evaluate right operand
				right = evaluate_expr(right,context);
				
				// Compute operator result
				new_token = ops_desc.at(op->op).impl(left,right);
			}
		}
		catch(Exception &e)
		{
			e.error += " while evaluating operator" + op->LogInitialPosition();
			delete op;
			delete left;
			delete right;
			throw e;
		}
		
		// Replace value in expression
		delete op;
		delete left;
		delete right;
		expr->expr_tokens.insert(expr->expr_tokens.begin()+minop_index-1,new_token);
	}
	
	// Resolve expressions, functions, node names and attribute names
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		TOKEN_TYPE token_type = expr->expr_tokens.at(i)->GetType();
		Token *val = 0;
		int replace_from, replace_to;
		
		if(token_type==EXPR)
		{
			// Recursively parse expressions
			val = evaluate_expr(expr->expr_tokens.at(i),context);
			replace_from = replace_to = i;
		}
		else if(token_type==FUNC)
		{
			TokenSeq left_context;
			val = evaluate_func(expr->expr_tokens,i,context,&left_context);
			replace_from = replace_to = i;
		}
		else if(token_type==NODENAME)
		{
			// Single node name (not precedeed by / or //). Compute based on current context (relative path)
			val = evaluate_node(expr->expr_tokens,i,context,false);
			replace_from = replace_to = i;
		}
		else if(token_type==AXIS)
		{
			// Single node name (not precedeed by / or //). Compute based on current context (relative path)
			val = evaluate_axis(expr->expr_tokens,i,context,false);
			replace_from = replace_to = i;
		}
		else if(token_type==ATTRNAME)
		{
			// Single attribute name (not precedeed by / or //). Compute based on current context (relative path)
			val = evaluate_attribute(expr->expr_tokens,i,context,false);
			replace_from = replace_to = i;
		}
		else if(token_type==SLASH || token_type==DSLASH)
		{
			// Node or attribute name precedeed by '/' or '//'
			bool depth = (token_type==DSLASH?true:false);
			TokenSeq abs_context(new TokenNode((DOMNode)(*xmldoc)));
			TokenSeq *left_context;
			if(i>0 && expr->expr_tokens.at(i-1)->GetType()==SEQ)
			{
				left_context = (TokenSeq *)expr->expr_tokens.at(i-1); // Compute based on preceding node list.
				replace_from = i-1;
			}
			else
			{
				left_context = &abs_context; // Absolute path
				replace_from = i;
			}
			
			if(i+1<expr->expr_tokens.size() && expr->expr_tokens.at(i+1)->GetType()==NODENAME)
				val = evaluate_node(expr->expr_tokens,i+1,left_context,depth);
			else if(i+1<expr->expr_tokens.size() && expr->expr_tokens.at(i+1)->GetType()==AXIS)
				val = evaluate_axis(expr->expr_tokens,i+1,left_context,depth);
			else if(i+1<expr->expr_tokens.size() && expr->expr_tokens.at(i+1)->GetType()==ATTRNAME)
				val = evaluate_attribute(expr->expr_tokens,i+1,left_context,depth);
			else if(i+1<expr->expr_tokens.size() && expr->expr_tokens.at(i+1)->GetType()==FUNC)
				val = evaluate_func(expr->expr_tokens,i+1,context,left_context);
			else
				throw Exception("XPath Eval","Missing node, attribute or function name after slash"+expr->expr_tokens.at(i)->LogInitialPosition());
			
			replace_to = i+1;
		}
		else if(token_type==FILTER)
		{
			if(i<=0 || expr->expr_tokens.at(i-1)->GetType()!=SEQ)
				throw Exception("XPath Eval","Filters can only be applied on sequences");
			
			TokenSeq *seq = (TokenSeq *)expr->expr_tokens.at(i-1);
			TokenFilter *filter = (TokenFilter *)expr->expr_tokens.at(i);
			
			if(filter->filter->expr_tokens.size()==1 && filter->filter->expr_tokens.at(0)->GetType()==LIT_INT)
				get_nth_token_node_list(seq,((TokenInt *)filter->filter->expr_tokens.at(0))->i);
			else
				filter_token_node_list(seq,filter->filter);
			
			delete  expr->expr_tokens.at(i);
			expr->expr_tokens.erase(expr->expr_tokens.begin()+i);
			i--;
		}
		
		if(val)
		{
			if(token_type!=EXPR)
			{
				val->SetInitialPosition(expr->expr_tokens.at(replace_to)->GetInitialPosition());
				for(int j=replace_from;j<=replace_to;j++)
					delete expr->expr_tokens.at(j);
			}
			expr->expr_tokens.erase(expr->expr_tokens.begin()+replace_from,expr->expr_tokens.begin()+replace_to+1);
			expr->expr_tokens.insert(expr->expr_tokens.begin()+replace_from,val);
			i = replace_from;
		}
	}
	
	if(expr->expr_tokens.size()!=1)
		throw Exception("XPath Eval","Error evaluating expression");
	
	Token *ret = expr->expr_tokens.at(0)->clone();
	delete expr;
	
	return ret;
}

XPathEval::XPathEval(DOMDocument *xmldoc)
{
	this->xmldoc = xmldoc;
	
	// Initialize operators
	ops_desc.insert(ops_desc.begin()+OPERATOR::MULT,{0,XPathOperators::Operator_MULT});
	ops_desc.insert(ops_desc.begin()+OPERATOR::DIV,{0,XPathOperators::Operator_DIV});
	ops_desc.insert(ops_desc.begin()+OPERATOR::MOD,{0,XPathOperators::Operator_MOD});
	ops_desc.insert(ops_desc.begin()+OPERATOR::PLUS,{1,XPathOperators::Operator_PLUS});
	ops_desc.insert(ops_desc.begin()+OPERATOR::MINUS,{1,XPathOperators::Operator_MINUS});
	ops_desc.insert(ops_desc.begin()+OPERATOR::LT,{2,XPathOperators::Operator_LT});
	ops_desc.insert(ops_desc.begin()+OPERATOR::LEQ,{2,XPathOperators::Operator_LEQ});
	ops_desc.insert(ops_desc.begin()+OPERATOR::GT,{2,XPathOperators::Operator_GT});
	ops_desc.insert(ops_desc.begin()+OPERATOR::GEQ,{2,XPathOperators::Operator_GEQ});
	ops_desc.insert(ops_desc.begin()+OPERATOR::EQ,{3,XPathOperators::Operator_EQ});
	ops_desc.insert(ops_desc.begin()+OPERATOR::NEQ,{3,XPathOperators::Operator_NEQ});
	ops_desc.insert(ops_desc.begin()+OPERATOR::AND,{4,XPathOperators::Operator_AND});
	ops_desc.insert(ops_desc.begin()+OPERATOR::OR,{5,XPathOperators::Operator_OR});
	ops_desc.insert(ops_desc.begin()+OPERATOR::PIPE,{5,XPathOperators::Operator_PIPE});
	
	// Initialize functions
	funcs_desc.insert(pair<string,func_desc>("true",{XPathFunctions::fntrue,0}));
	funcs_desc.insert(pair<string,func_desc>("false",{XPathFunctions::fnfalse,0}));
	funcs_desc.insert(pair<string,func_desc>("not",{XPathFunctions::fnnot,0}));
	funcs_desc.insert(pair<string,func_desc>("name",{XPathFunctions::name,0}));
	funcs_desc.insert(pair<string,func_desc>("count",{XPathFunctions::count,0}));
	funcs_desc.insert(pair<string,func_desc>("min",{XPathFunctions::min,0}));
	funcs_desc.insert(pair<string,func_desc>("max",{XPathFunctions::max,0}));
	funcs_desc.insert(pair<string,func_desc>("position",{XPathFunctions::position,0}));
	funcs_desc.insert(pair<string,func_desc>("last",{XPathFunctions::last,0}));
	funcs_desc.insert(pair<string,func_desc>("string-length",{XPathFunctions::string_length,0}));
	funcs_desc.insert(pair<string,func_desc>("substring",{XPathFunctions::substring,0}));
	funcs_desc.insert(pair<string,func_desc>("contains",{XPathFunctions::contains,0}));
	funcs_desc.insert(pair<string,func_desc>("string-join",{XPathFunctions::string_join,0}));
}

void XPathEval::RegisterFunction(string name,func_desc f)
{
	funcs_desc.erase(name);
	funcs_desc.insert(pair<string,func_desc>(name,f));
}

Token *XPathEval::Evaluate(const string &xpath,DOMNode context)
{
	unique_ptr<TokenSeq> current_context_seq(new TokenSeq(new TokenNode(context)));
	eval_context current_context(current_context_seq.get());
	RegisterFunction("current",{XPathFunctions::current,current_context_seq.get()});
	
	XPathParser parser;
	TokenExpr *parsed_expr = 0;
	try
	{
		parsed_expr = parser.Parse(xpath);
		return evaluate_expr(parsed_expr,current_context);
	}
	catch(Exception &e)
	{
		if(parsed_expr)
			delete parsed_expr;
		throw e;
	}
}

void XPathEval::Parse(const std::string &xpath)
{
	XPathParser parser;
	TokenExpr *parsed_expr = 0;
	try
	{
		parsed_expr = parser.Parse(xpath);
	}
	catch(Exception &e)
	{
		if(parsed_expr)
			delete parsed_expr;
		throw e;
	}
	
	delete parsed_expr;
}
