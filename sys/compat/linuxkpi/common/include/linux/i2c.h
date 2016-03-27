
extern int i2c_del_adapter(struct i2c_adapter *);

extern int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
                        int num);

extern int __i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
                          int num);
