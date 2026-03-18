mod gen;

use gen::*;
use std::collections::HashMap;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_basic_struct_roundtrip() {
        let ts = TestStruct {
            int_val: 42,
            long_val: 1234567890,
            float_val: 3.14,
            double_val: 2.718281828,
            bool_val: true,
            sstr_val: "hello world".to_string(),
        };
        let json = serde_json::to_string(&ts).unwrap();
        let parsed: TestStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(ts, parsed);
    }

    #[test]
    fn test_enum_roundtrip() {
        let ets = EnumTestStruct {
            color: Color::RED,
            status: Status::ACTIVE,
            value: 100,
            colors: vec![Color::GREEN, Color::BLUE],
        };
        let json = serde_json::to_string(&ets).unwrap();
        assert!(json.contains("\"RED\""));
        assert!(json.contains("\"ACTIVE\""));
        let parsed: EnumTestStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(ets, parsed);
    }

    #[test]
    fn test_optional_fields() {
        let opt = OptionalOnlyStruct {
            id: 1,
            name: Some("test".to_string()),
            score: None,
            active: Some(true),
            rating: None,
            precise: None,
            big_num: None,
        };
        let json = serde_json::to_string(&opt).unwrap();
        assert!(!json.contains("\"score\""));
        assert!(json.contains("\"name\""));
        let parsed: OptionalOnlyStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(opt, parsed);
    }

    #[test]
    fn test_nullable_fields() {
        let ns = NullableOnlyStruct {
            id: 1,
            name: None,
            score: Some(42),
            active: None,
        };
        let json = serde_json::to_string(&ns).unwrap();
        assert!(json.contains("null"));
        let parsed: NullableOnlyStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(ns, parsed);
    }

    #[test]
    fn test_json_alias() {
        let ab = AliasBasic {
            username: "alice".to_string(),
            created: 1700000000,
            id: 42,
        };
        let json = serde_json::to_string(&ab).unwrap();
        assert!(json.contains("\"user_name\""));
        assert!(json.contains("\"created_at\""));
        assert!(!json.contains("\"username\""));
        let parsed: AliasBasic = serde_json::from_str(&json).unwrap();
        assert_eq!(ab, parsed);
    }

    #[test]
    fn test_nested_struct() {
        let ns = NestedStruct {
            id: 1,
            name: "test".to_string(),
            embedded: TestStruct {
                int_val: 10,
                long_val: 20,
                float_val: 1.0,
                double_val: 2.0,
                bool_val: false,
                sstr_val: "nested".to_string(),
            },
        };
        let json = serde_json::to_string(&ns).unwrap();
        let parsed: NestedStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(ns, parsed);
    }

    #[test]
    fn test_dynamic_arrays() {
        let cs = ComplexStruct {
            simple_int: 1,
            simple_long: 2,
            simple_float: 3.0,
            simple_double: 4.0,
            simple_bool: true,
            simple_string: "hi".to_string(),
            int_array: vec![1, 2, 3],
            long_array: vec![10, 20],
            float_array: vec![1.1, 2.2],
            double_array: vec![3.3, 4.4],
            string_array: vec!["a".to_string(), "b".to_string()],
            address: House {
                number: "42".to_string(),
                street: "Main St".to_string(),
            },
            contacts: vec![Person {
                name: "Bob".to_string(),
                age: "30".to_string(),
            }],
        };
        let json = serde_json::to_string(&cs).unwrap();
        let parsed: ComplexStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(cs, parsed);
    }

    #[test]
    fn test_fixed_arrays() {
        let fa = FixedArrayStruct {
            fixed_ints: [1, 2, 3, 4, 5],
            fixed_longs: [10, 20, 30],
            fixed_floats: [1.0, 2.0, 3.0, 4.0],
            fixed_doubles: [1.1, 2.2, 3.3],
            fixed_strings: [
                "a".to_string(),
                "b".to_string(),
                "c".to_string(),
            ],
            fixed_bools: [true, false],
            fixed_colors: [Color::RED, Color::GREEN, Color::BLUE],
            fixed_contacts: [
                Person { name: "A".to_string(), age: "1".to_string() },
                Person { name: "B".to_string(), age: "2".to_string() },
            ],
        };
        let json = serde_json::to_string(&fa).unwrap();
        let parsed: FixedArrayStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(fa, parsed);
    }

    #[test]
    fn test_map_roundtrip() {
        let mut scores = HashMap::new();
        scores.insert("alice".to_string(), 100);
        scores.insert("bob".to_string(), 200);
        let ms = MapIntStruct { scores };
        let json = serde_json::to_string(&ms).unwrap();
        let parsed: MapIntStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(ms, parsed);
    }

    #[test]
    fn test_default_values() {
        let db = DefaultBasic::default();
        assert_eq!(db.count, 42);
        assert_eq!(db.big, 1000000);
        assert!((db.ratio - 3.14).abs() < 0.001);
        assert!((db.precise - 2.718281828).abs() < 0.000001);
        assert!(db.active);
        assert_eq!(db.label, "hello");
    }

    #[test]
    fn test_enum_defaults() {
        let de = DefaultEnum::default();
        assert_eq!(de.color, Color::GREEN);
        assert_eq!(de.status, Status::PENDING);
    }

    #[test]
    fn test_negative_defaults() {
        let dn = DefaultNegative::default();
        assert_eq!(dn.neg_int, -10);
        assert_eq!(dn.neg_long, -99999);
        assert!((dn.neg_float - (-1.5_f32)).abs() < 0.001);
        assert!((dn.neg_double - (-0.001_f64)).abs() < 0.000001);
    }

    #[test]
    fn test_oneof_roundtrip() {
        let shape = Shape::Circle(Circle { radius: 5.0 });
        let json = serde_json::to_string(&shape).unwrap();
        assert!(json.contains("\"type\":\"circle\"") || json.contains("\"type\": \"circle\""));
        let parsed: Shape = serde_json::from_str(&json).unwrap();
        assert_eq!(shape, parsed);
    }

    #[test]
    fn test_oneof_rectangle() {
        let shape = Shape::Rectangle(Rectangle { width: 10.0, height: 20.0 });
        let json = serde_json::to_string(&shape).unwrap();
        let parsed: Shape = serde_json::from_str(&json).unwrap();
        assert_eq!(shape, parsed);
    }

    #[test]
    fn test_drawing_with_shapes() {
        let drawing = Drawing {
            name: "test".to_string(),
            shape: Shape::Triangle(Triangle {
                base: 3.0,
                height: 4.0,
                label: "tri".to_string(),
            }),
            shapes: vec![
                Shape::Circle(Circle { radius: 1.0 }),
                Shape::Rectangle(Rectangle { width: 2.0, height: 3.0 }),
            ],
        };
        let json = serde_json::to_string(&drawing).unwrap();
        let parsed: Drawing = serde_json::from_str(&json).unwrap();
        assert_eq!(drawing, parsed);
    }

    #[test]
    fn test_precise_ints() {
        let pi = PreciseInts {
            i8: -128,
            i16: -32768,
            i32: -2147483648,
            i64: -9223372036854775808,
            u8: 255,
            u16: 65535,
            u32: 4294967295,
            u64: 18446744073709551615,
        };
        let json = serde_json::to_string(&pi).unwrap();
        let parsed: PreciseInts = serde_json::from_str(&json).unwrap();
        assert_eq!(pi, parsed);
    }

    #[test]
    fn test_c_json_compatibility() {
        let json = r#"{"int_val":42,"long_val":100,"float_val":3.14,"double_val":2.718,"bool_val":true,"sstr_val":"test"}"#;
        let parsed: TestStruct = serde_json::from_str(json).unwrap();
        assert_eq!(parsed.int_val, 42);
        assert_eq!(parsed.long_val, 100);
        assert!(parsed.bool_val);
        assert_eq!(parsed.sstr_val, "test");
    }

    #[test]
    fn test_optional_from_partial_json() {
        let json = r#"{"id":1}"#;
        let parsed: OptionalOnlyStruct = serde_json::from_str(json).unwrap();
        assert_eq!(parsed.id, 1);
        assert_eq!(parsed.name, None);
        assert_eq!(parsed.score, None);
    }

    #[test]
    fn test_map_all_types() {
        let mut int_map = HashMap::new();
        int_map.insert("a".to_string(), 1);
        let mut str_map = HashMap::new();
        str_map.insert("b".to_string(), "hello".to_string());
        let mut bool_map = HashMap::new();
        bool_map.insert("c".to_string(), true);
        let mut float_map = HashMap::new();
        float_map.insert("d".to_string(), 1.5_f32);
        let mut double_map = HashMap::new();
        double_map.insert("e".to_string(), 2.5_f64);
        let mut long_map = HashMap::new();
        long_map.insert("f".to_string(), 100_i64);
        let mut enum_map = HashMap::new();
        enum_map.insert("g".to_string(), Color::RED);
        let mut struct_map = HashMap::new();
        struct_map.insert("h".to_string(), Person { name: "Z".to_string(), age: "1".to_string() });

        let mat = MapAllTypesStruct {
            int_map,
            long_map,
            float_map,
            double_map,
            bool_map,
            str_map,
            enum_map,
            struct_map,
        };
        let json = serde_json::to_string(&mat).unwrap();
        let parsed: MapAllTypesStruct = serde_json::from_str(&json).unwrap();
        assert_eq!(mat, parsed);
    }

    #[test]
    fn test_serde_default_on_missing_field() {
        let json = r#"{"no_default":0}"#;
        let parsed: DefaultBasic = serde_json::from_str(json).unwrap();
        assert_eq!(parsed.count, 42);
        assert_eq!(parsed.big, 1000000);
        assert_eq!(parsed.no_default, 0);
    }
}

fn main() {
    println!("Rust code generation tests — run with `cargo test`");
}
