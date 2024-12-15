expr : factor ['+++' expr]
    | factor ['---' expr]
factor : INT { '***' INT } 
