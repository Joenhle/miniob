
%{

#include "sql/parser/parse_defs.h"
#include "sql/parser/yacc_sql.tab.h"
#include "sql/parser/lex.yy.h"
// #include "common/log/log.h" // 包含C++中的头文件

int yydebug = 1;

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

typedef struct ParserContext {
  Query * ssql;
  size_t select_length;
  size_t condition_length;
  size_t from_length;
  size_t value_length;
  Value values[MAX_NUM];
  Condition conditions[MAX_NUM];
  CompOp comp;
  char id[MAX_NUM];

  size_t expr_cell_buffer_condition_first_position;
  size_t expr_cell_buffer_num;
  ExprCellBuffer expr_cells[100];

} ParserContext;

//获取子串
char *substr(const char *s,int n1,int n2)/*从s中提取下标为n1~n2的字符组成一个新字符串，然后返回这个新串的首地址*/
{
  char *sp = malloc(sizeof(char) * (n2 - n1 + 2));
  int i, j = 0;
  for (i = n1; i <= n2; i++) {
    sp[j++] = s[i];
  }
  sp[j] = 0;
  return sp;
}

void yyerror(yyscan_t scanner, const char *str)
{
  ParserContext *context = (ParserContext *)(yyget_extra(scanner));
  query_reset(context->ssql);
  context->ssql->flag = SCF_ERROR;
  context->condition_length = 0;
  context->from_length = 0;
  context->select_length = 0;
  context->value_length = 0;
  context->ssql->sstr.insertion.value_num = 0;
  printf("parse sql failed. error=%s", str);
}

ParserContext *get_context(yyscan_t scanner)
{
  return (ParserContext *)yyget_extra(scanner);
}

#define CONTEXT get_context(scanner)

%}

%define api.pure full
%lex-param { yyscan_t scanner }
%parse-param { void *scanner }

//标识tokens
%token  SEMICOLON
        CREATE
        DROP
        TABLE
        TABLES
        INDEX
        SELECT
        DESC
        ASC
        SHOW
        SYNC
        INSERT
        DELETE
        UPDATE
        LBRACE
        RBRACE
        COMMA
        TRX_BEGIN
        TRX_COMMIT
        TRX_ROLLBACK
        DATE_T
        INT_T
        STRING_T
        FLOAT_T
        TEXT_T
        HELP
        EXIT
        DOT //QUOTE
        INTO
        VALUES
        FROM
        INNER
        JOIN
        WHERE
        AND
        SET
        ON
        LOAD
        DATA
        INFILE
        IS_
        NOT
        LIKE_
        GROUP
        BY
        ORDER
        HAVING
        AS
        NULLABLE
        NULL_
        EQ
        LT
        GT
        LE
        GE
        NE
        PLUS
        DIV
        MINUS
        UNIQUE

%union {
  struct _Attr *attr;
  struct _Condition *condition1;
  struct _Value *value1;
  char *string;
  int number;
  float floats;
	char *position;
}

%token <number> NUMBER
%token <floats> FLOAT
%token <string> ID
%token <string> AGGREGATION_FUNC
%token <string> AGGREGATION_FUNC_VALUE
%token <string> PATH
%token <string> SSS
%token <string> STAR
%token <string> STRING_V
%token <string> DATE_STR
//非终结符

%type <number> type;
%type <condition1> condition;
%type <value1> value;
%type <number> number;
%type <number> od_type;

%left PLUS MINUS
%left STAR DIV
%right U_neg



%%

commands:		//commands or sqls. parser starts here.
    /* empty */
    | commands command
    ;

command:
	  select
	| insert
	| update
	| delete
	| create_table
	| drop_table
	| show_tables
	| desc_table
	| create_index
	| drop_index
	| show_index
	| sync
	| begin
	| commit
	| rollback
	| load_data
	| help
	| exit
    ;

exit:
    EXIT SEMICOLON {
        CONTEXT->ssql->flag=SCF_EXIT;//"exit";
    };

help:
    HELP SEMICOLON {
        CONTEXT->ssql->flag=SCF_HELP;//"help";
    };

sync:
    SYNC SEMICOLON {
      CONTEXT->ssql->flag = SCF_SYNC;
    }
    ;

begin:
    TRX_BEGIN SEMICOLON {
      CONTEXT->ssql->flag = SCF_BEGIN;
    }
    ;

commit:
    TRX_COMMIT SEMICOLON {
      CONTEXT->ssql->flag = SCF_COMMIT;
    }
    ;

rollback:
    TRX_ROLLBACK SEMICOLON {
      CONTEXT->ssql->flag = SCF_ROLLBACK;
    }
    ;

drop_table:		/*drop table 语句的语法解析树*/
    DROP TABLE ID SEMICOLON {
        CONTEXT->ssql->flag = SCF_DROP_TABLE;//"drop_table";
        drop_table_init(&CONTEXT->ssql->sstr.drop_table, $3);
    };

show_tables:
    SHOW TABLES SEMICOLON {
      CONTEXT->ssql->flag = SCF_SHOW_TABLES;
    }
    ;

desc_table:
    DESC ID SEMICOLON {
      CONTEXT->ssql->flag = SCF_DESC_TABLE;
      desc_table_init(&CONTEXT->ssql->sstr.desc_table, $2);
    }
    ;

index_field_list:
    ID index_field_list_ {
            create_index_append_attr(&CONTEXT->ssql->sstr.create_index, $1);
    }
    ;
index_field_list_:
    /* empty */
        {
			create_index_init(&CONTEXT->ssql->sstr.create_index);
        }
        ;
    | COMMA ID index_field_list_ {
            create_index_append_attr(&CONTEXT->ssql->sstr.create_index, $2);
    }
    ;

create_index:		/*create index 语句的语法解析树*/
    CREATE INDEX ID ON ID LBRACE index_field_list RBRACE SEMICOLON
		{
			CONTEXT->ssql->flag = SCF_CREATE_INDEX;//"create_index";
			CONTEXT->ssql->sstr.create_index.is_unique = 0;
			create_index_set_relation_name(&CONTEXT->ssql->sstr.create_index, $5);
			create_index_set_index_name(&CONTEXT->ssql->sstr.create_index, $3);
		}
    | CREATE UNIQUE INDEX ID ON ID LBRACE index_field_list RBRACE SEMICOLON
		{
			CONTEXT->ssql->flag = SCF_CREATE_INDEX;//"create_index";
			CONTEXT->ssql->sstr.create_index.is_unique = 1;
			create_index_set_relation_name(&CONTEXT->ssql->sstr.create_index, $6);
			create_index_set_index_name(&CONTEXT->ssql->sstr.create_index, $4);
		}
    ;

drop_index:			/*drop index 语句的语法解析树*/
    DROP INDEX ID  SEMICOLON
		{
			CONTEXT->ssql->flag=SCF_DROP_INDEX;//"drop_index";
			drop_index_init(&CONTEXT->ssql->sstr.drop_index, $3);
		}
    ;

show_index:
    SHOW INDEX FROM ID SEMICOLON
        {
            CONTEXT->ssql->flag=SCF_SHOW_INDEX;
            show_index_init(&CONTEXT->ssql->sstr.show_index, $4);
        }
    ;


create_table:		/*create table 语句的语法解析树*/
    CREATE TABLE ID LBRACE attr_def attr_def_list RBRACE SEMICOLON
		{
			CONTEXT->ssql->flag=SCF_CREATE_TABLE;//"create_table";
			// CONTEXT->ssql->sstr.create_table.attribute_count = CONTEXT->value_length;
			create_table_init_name(&CONTEXT->ssql->sstr.create_table, $3);
			//临时变量清零
			CONTEXT->value_length = 0;
		}
    ;
attr_def_list:
    /* empty */
    | COMMA attr_def attr_def_list {    }
    ;

attr_def:
    ID_get type LBRACE number RBRACE
		{
			AttrInfo attribute;
			attr_info_init(&attribute, CONTEXT->id, $2, $4, 0);
			create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
			// CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name =(char*)malloc(sizeof(char));
			// strcpy(CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name, CONTEXT->id);
			// CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].type = $2;
			// CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].length = $4;
			CONTEXT->value_length++;
		}
    |ID_get type
		{
			AttrInfo attribute;
			attr_info_init(&attribute, CONTEXT->id, $2, 4, 0);
			create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
			// CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name=(char*)malloc(sizeof(char));
			// strcpy(CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name, CONTEXT->id);
			// CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].type=$2;
			// CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].length=4; // default attribute length
			CONTEXT->value_length++;
		}
    |ID_get type NULLABLE LBRACE number RBRACE
        {
            AttrInfo attribute;
            attr_info_init(&attribute, CONTEXT->id, $2, $5, 1);
            create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
            // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name =(char*)malloc(sizeof(char));
            // strcpy(CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name, CONTEXT->id);
            // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].type = $2;
            // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].length = $4;
            CONTEXT->value_length++;
        }
    |ID_get type NULLABLE
        {
            AttrInfo attribute;
            attr_info_init(&attribute, CONTEXT->id, $2, 4, 1);
            create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
            // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name=(char*)malloc(sizeof(char));
            // strcpy(CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name, CONTEXT->id);
            // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].type=$2;
            // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].length=4; // default attribute length
            CONTEXT->value_length++;
        }
        |ID_get type NOT NULL_ LBRACE number RBRACE
            {
                AttrInfo attribute;
                attr_info_init(&attribute, CONTEXT->id, $2, $6, 0);
                create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
                // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name =(char*)malloc(sizeof(char));
                // strcpy(CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name, CONTEXT->id);
                // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].type = $2;
                // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].length = $4;
                CONTEXT->value_length++;
            }
        |ID_get type NOT NULL_
            {
                AttrInfo attribute;
                attr_info_init(&attribute, CONTEXT->id, $2, 4, 0);
                create_table_append_attribute(&CONTEXT->ssql->sstr.create_table, &attribute);
                // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name=(char*)malloc(sizeof(char));
                // strcpy(CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].name, CONTEXT->id);
                // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].type=$2;
                // CONTEXT->ssql->sstr.create_table.attributes[CONTEXT->value_length].length=4; // default attribute length
                CONTEXT->value_length++;
            }

    ;
number:
		NUMBER {$$ = $1;}
		;
type:
	INT_T { $$=INTS; }
       | STRING_T { $$=CHARS; }
       | FLOAT_T  { $$=FLOATS; }
       | DATE_T   { $$=DATES; }
       | TEXT_T   { $$=TEXT; }
       ;
ID_get:
	ID
	{
		char *temp=$1;
		snprintf(CONTEXT->id, sizeof(CONTEXT->id), "%s", temp);
	}
	;


insert:				/*insert   语句的语法解析树*/
    INSERT INTO ID VALUES value_tuple value_tuple_list SEMICOLON
		{
			// CONTEXT->values[CONTEXT->value_length++] = *$6;

			CONTEXT->ssql->flag=SCF_INSERT;//"insert";
			// CONTEXT->ssql->sstr.insertion.relation_name = $3;
			// CONTEXT->ssql->sstr.insertion.value_num = CONTEXT->value_length;
			// for(i = 0; i < CONTEXT->value_length; i++){
			// 	CONTEXT->ssql->sstr.insertion.values[i] = CONTEXT->values[i];
      // }
            inserts_init(&CONTEXT->ssql->sstr.insertion, $3, CONTEXT->values, CONTEXT->value_length);
      //临时变量清零
      CONTEXT->value_length=0;
    }

value_tuple:
    LBRACE value value_list RBRACE {
        CONTEXT->ssql->sstr.insertion.tuple_size[CONTEXT->ssql->sstr.insertion.tuple_num]++;
        CONTEXT->ssql->sstr.insertion.tuple_num++;
    }
    ;

value_tuple_list:
    /* empty */
    | COMMA value_tuple value_tuple_list {
    }
    ;

value_list:
    /* empty */
    | COMMA value value_list  {
  		CONTEXT->ssql->sstr.insertion.tuple_size[CONTEXT->ssql->sstr.insertion.tuple_num]++;
	  }
    ;
value:
    NUMBER{
  		value_init_integer(&CONTEXT->values[CONTEXT->value_length++], $1);
		}
    |FLOAT{
  		value_init_float(&CONTEXT->values[CONTEXT->value_length++], $1);
		}
	|DATE_STR{
            $1 = substr($1,1,strlen($1)-2);
  		int ret = value_init_date(&CONTEXT->values[CONTEXT->value_length++], $1);
  		if (ret){
			// TODO: find an elegant way to do this
			CONTEXT->ssql->flag = SCF_ERROR;
			return -1;
        }
	    }
    |NULL_ {
            CONTEXT->value_length++;
        }
    |SSS {
			$1 = substr($1,1,strlen($1)-2);
  		value_init_string(&CONTEXT->values[CONTEXT->value_length++], $1);
		}
    ;

delete:		/*  delete 语句的语法解析树*/
    DELETE FROM ID where SEMICOLON
		{
			CONTEXT->ssql->flag = SCF_DELETE;//"delete";
			deletes_init_relation(&CONTEXT->ssql->sstr.deletion, $3);
			deletes_set_conditions(&CONTEXT->ssql->sstr.deletion,
					CONTEXT->conditions, CONTEXT->condition_length);
			CONTEXT->condition_length = 0;	
    }
    ;
update:			/*  update 语句的语法解析树*/
    UPDATE ID SET ID EQ value more_update where SEMICOLON
		{
			CONTEXT->ssql->flag = SCF_UPDATE;//"update";
			Value *value = &CONTEXT->values[0];
			updates_init(&CONTEXT->ssql->sstr.update, $2, $4, value,
					CONTEXT->conditions, CONTEXT->condition_length);
			CONTEXT->condition_length = 0;
		}
    ;

more_update:
    /* empty */
    |COMMA ID EQ value more_update
        {
            Value *value = &CONTEXT->values[CONTEXT->value_length-1-CONTEXT->ssql->sstr.update.update_attr_num];
            updates_append(&CONTEXT->ssql->sstr.update,$2,value);
            printf("%d",CONTEXT->value_length-1);
        }
    ;


select:				/*  select 语句的语法解析树*/
    SELECT select_list FROM rel where group_by having order_by SEMICOLON
		{
		    printf("have done");
			// CONTEXT->ssql->sstr.selection.relations[CONTEXT->from_length++]=$4;


			selects_append_conditions(&CONTEXT->ssql->sstr.selection, CONTEXT->conditions, CONTEXT->condition_length);

			CONTEXT->ssql->flag=SCF_SELECT;//"select";
			// CONTEXT->ssql->sstr.selection.attr_num = CONTEXT->select_length;

			//临时变量清零
			CONTEXT->condition_length=0;
			CONTEXT->from_length=0;
			CONTEXT->select_length=0;
			CONTEXT->value_length = 0;
	}
	;

select_list:
    STAR {
            RelAttr attr;
            relation_attr_init(&attr, NULL, "*", NULL);
            selects_append_attribute(&CONTEXT->ssql->sstr.selection, &attr);
        }
    | STAR COMMA selects_expression_all {
            RelAttr attr;
            relation_attr_init(&attr, NULL, "*", NULL);
            selects_append_attribute(&CONTEXT->ssql->sstr.selection, &attr);
    }
    | selects_expression_all {
      }
    ;

selects_expression_all:
    expr {
            //把存在buffer中的expr_cells转换为attribute放进sql中
            append_buffer_expr_to_select_attribute(&CONTEXT->ssql->sstr.selection, &CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_num);
            //把存在buffer中的expr_cells放进sql的ExprList中
            append_buffer_expr_to_select_exprlist(&CONTEXT->ssql->sstr.selection.expr_list, &CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_num);
            CONTEXT->ssql->sstr.selection.expr_list.exprs_num++;
            //清楚expr_cells缓存
            clear_buffer_expr_cell_list(&CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_num);
            CONTEXT->expr_cell_buffer_num = 0;
        }
    | expr AS ID {
            append_buffer_expr_to_select_attribute(&CONTEXT->ssql->sstr.selection, &CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_num);
            append_buffer_expr_to_select_exprlist(&CONTEXT->ssql->sstr.selection.expr_list, &CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_num);
            append_alias_to_expr(&CONTEXT->ssql->sstr.selection.expr_list, $3);
            CONTEXT->ssql->sstr.selection.expr_list.exprs_num++;
            clear_buffer_expr_cell_list(&CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_num);
            CONTEXT->expr_cell_buffer_num = 0;
        }
    | selects_expression_all COMMA selects_expression_all {
        }
    ;

expr:   expr PLUS  expr    {set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 8, "+", NULL, NULL);}
    |   expr MINUS expr    {set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 9, "-", NULL, NULL);}
    |   expr STAR  expr    {set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 10, "*", NULL, NULL);}
    |   expr DIV   expr    {set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 11, "/", NULL, NULL);}
    |   expr NUMBER        {
                                value_init_integer(&CONTEXT->values[CONTEXT->value_length++], $2 * -1);
                                char* temp = value_to_string(&CONTEXT->values[CONTEXT->value_length-1]);
                                set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num], 13, temp, NULL, NULL);
                                set_buffer_expr_cell_value_pos(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], CONTEXT->value_length-1);
                                set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 13, "-", NULL, NULL);
                            }
    |   LBRACE expr RBRACE {}
    |   MINUS expr %prec U_neg {set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 12, "-", NULL, NULL);}
    |   expr_cell {}
    ;

//先把信息都存入context buffer
expr_cell:
    ID {
            set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 1, $1, NULL, NULL);
       }
    | ID DOT ID {
            set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 2, $1, $3, NULL);
       }
    | AGGREGATION_FUNC LBRACE ID RBRACE {
            set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 3, $1, $3, NULL);
       }
    | AGGREGATION_FUNC LBRACE ID DOT ID RBRACE {
            set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 4, $1, $3, $5);
       }
    | AGGREGATION_FUNC LBRACE STAR RBRACE {
            if (strcmp($1, "count") != 0 && strcmp($1, "COUNT") != 0) {
            			CONTEXT->ssql->flag = SCF_ERROR;
            			return -1;
            }
            set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 5, $1, NULL, NULL);
       }
    | AGGREGATION_FUNC LBRACE number RBRACE {
            char num[20];
            sprintf(num, "%d", $3);
            if ((strcmp($1, "count") != 0 && strcmp($1, "COUNT") != 0) || $3 != 1) {
                        CONTEXT->ssql->flag = SCF_ERROR;
                        return -1;
            }
            set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], 6, $1, num, NULL);
       }
    | value {
            char* temp = value_to_string(&CONTEXT->values[CONTEXT->value_length-1]);
            set_buffer_expr_cell(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num], 7, temp, NULL, NULL);
            set_buffer_expr_cell_value_pos(&CONTEXT->expr_cells[CONTEXT->expr_cell_buffer_num++], CONTEXT->value_length-1);
    }
    ;

rel:
   ID rel_list {
            selects_append_relation(&CONTEXT->ssql->sstr.selection, $1, NULL);
        }
   | ID AS ID rel_list {
            selects_append_relation(&CONTEXT->ssql->sstr.selection, $1, $3);
   }
   ;

rel_list:
    /* empty */
    | COMMA ID rel_list {
				selects_append_relation(&CONTEXT->ssql->sstr.selection, $2, NULL);
		  }
    | COMMA ID AS ID rel_list {
            selects_append_relation(&CONTEXT->ssql->sstr.selection, $2, $4);
        }
    | INNER JOIN ID rel_list {
              selects_append_relation(&CONTEXT->ssql->sstr.selection, $3, NULL);
          }
    | INNER JOIN ID AS ID rel_list {
            selects_append_relation(&CONTEXT->ssql->sstr.selection, $3, $5);
          }
    | INNER JOIN ID ON condition condition_list rel_list {
                selects_append_relation(&CONTEXT->ssql->sstr.selection, $3, NULL);
          }
    | INNER JOIN ID AS ID ON condition condition_list rel_list {
            selects_append_relation(&CONTEXT->ssql->sstr.selection, $3, $5);
        }
    ;

where:
    /* empty */
    | WHERE condition condition_list {
				// CONTEXT->conditions[CONTEXT->condition_length++]=*$2;
			}
    ;

condition:
    condition_expression_all comOp condition_expression_all {
        Condition condition;
        build_condition_by_buffer_expr(CONTEXT->comp, &CONTEXT->values, &condition, &CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_condition_first_position, CONTEXT->expr_cell_buffer_num);
        CONTEXT->conditions[CONTEXT->condition_length++] = condition;
        CONTEXT->expr_cell_buffer_condition_first_position = 0;
        clear_buffer_expr_cell_list(&CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_num);
        CONTEXT->expr_cell_buffer_num = 0;
    }
    ;

condition_list:
    /* empty */
    | AND condition condition_list {
                // CONTEXT->conditions[CONTEXT->condition_length++]=*$2;
            }
    ;
    
having:
    /* empty */
    | HAVING having_condition having_condition_list {
        }
    ;

having_condition:
    condition_expression_all comOp condition_expression_all {
        Condition condition;
        build_condition_by_buffer_expr(CONTEXT->comp, &CONTEXT->values, &condition, &CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_condition_first_position, CONTEXT->expr_cell_buffer_num);
        CONTEXT->ssql->sstr.selection.group_by.having_condition[CONTEXT->ssql->sstr.selection.group_by.having_condition_num++] = condition;
        CONTEXT->expr_cell_buffer_condition_first_position = 0;
        clear_buffer_expr_cell_list(&CONTEXT->expr_cells, CONTEXT->expr_cell_buffer_num);
        CONTEXT->expr_cell_buffer_num = 0;
    }
    ;
    
    
having_condition_list:
    /* empty */
    | AND having_condition having_condition_list {
            }
    ;
    

group_by:
    /* empty */
    | GROUP BY ID group_by_rel_list {
        RelAttr attr;
        relation_attr_init(&attr, NULL, $3, NULL);
        groupby_append_attribute(&CONTEXT->ssql->sstr.selection, &attr);
    }
    | GROUP BY ID DOT ID group_by_rel_list {
        RelAttr attr;
        relation_attr_init(&attr, $3, $5, NULL);
        groupby_append_attribute(&CONTEXT->ssql->sstr.selection, &attr);
    }
    ;


group_by_rel_list:
    /* empty */
    | COMMA ID group_by_rel_list {
        RelAttr attr;
        relation_attr_init(&attr, NULL, $2, NULL);
        groupby_append_attribute(&CONTEXT->ssql->sstr.selection, &attr);
    }
    | COMMA ID DOT ID group_by_rel_list {
        RelAttr attr;
        relation_attr_init(&attr, $2, $4, NULL);
        groupby_append_attribute(&CONTEXT->ssql->sstr.selection, &attr);
    }
    ;


order_by:
    /* empty */
    | ORDER BY ID od_type order_by_info {
        RelAttr attr;
        relation_attr_init(&attr, NULL, $3, NULL);
        selects_append_order_by_attr(&CONTEXT->ssql->sstr.selection,&attr,$4);
    }
    | ORDER BY ID DOT ID od_type order_by_info {
        RelAttr attr;
        relation_attr_init(&attr, $3, $5, NULL);
        selects_append_order_by_attr(&CONTEXT->ssql->sstr.selection,&attr,$6);
    }
    ;


od_type:
    /* empty */ { $$=ASC_ORDER; }
        | ASC { $$=ASC_ORDER; }
        | DESC { $$=DESC_ORDER; }
       ;


order_by_info:
    /* empty */
    | COMMA ID od_type order_by_info {
        RelAttr attr;
        relation_attr_init(&attr, NULL, $2, NULL);
        selects_append_order_by_attr(&CONTEXT->ssql->sstr.selection,&attr,$3);
    }
    | COMMA ID DOT ID od_type order_by_info {
        RelAttr attr;
        relation_attr_init(&attr, $2, $4, NULL);
        selects_append_order_by_attr(&CONTEXT->ssql->sstr.selection,&attr,$5);
    }
    ;



condition_expression_all:
    expr {
            if (CONTEXT->expr_cell_buffer_condition_first_position == 0) {
                CONTEXT->expr_cell_buffer_condition_first_position = CONTEXT->expr_cell_buffer_num;
            }
         }
    ;



comOp:
  	  EQ { CONTEXT->comp = EQUAL_TO; }
    | LT { CONTEXT->comp = LESS_THAN; }
    | GT { CONTEXT->comp = GREAT_THAN; }
    | LE { CONTEXT->comp = LESS_EQUAL; }
    | GE { CONTEXT->comp = GREAT_EQUAL; }
    | NE { CONTEXT->comp = NOT_EQUAL; }
    | NOT LIKE_ { CONTEXT->comp = NOT_LIKE; }
    | LIKE_ { CONTEXT->comp = LIKE; }
    | IS_ NOT { CONTEXT->comp = IS_NOT; }
    | IS_ {CONTEXT->comp = IS; }
    ;

load_data:
		LOAD DATA INFILE SSS INTO TABLE ID SEMICOLON
		{
		  CONTEXT->ssql->flag = SCF_LOAD_DATA;
			load_data_init(&CONTEXT->ssql->sstr.load_data, $7, $4);
		}
		;
%%
//_____________________________________________________________________
extern void scan_string(const char *str, yyscan_t scanner);

int sql_parse(const char *s, Query *sqls){
	ParserContext context;
	memset(&context, 0, sizeof(context));
	yyscan_t scanner;
	yylex_init_extra(&context, &scanner);
	context.ssql = sqls;
	scan_string(s, scanner);
	memset(context.ssql->sstr.selection.expr_list.exprs, 0, 20 * 20 * 20 * sizeof(char));
	int result = yyparse(scanner);
	yylex_destroy(scanner);
	return result;
}
