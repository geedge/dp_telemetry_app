/**
 * Copyright (c) 2015-2016, Chill Zhuang 庄骞 (smallchill@163.com).
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package cn.nis.ntc.utils;


import cn.hutool.core.collection.CollectionUtil;
import cn.hutool.core.util.ObjectUtil;
import cn.hutool.core.util.StrUtil;
import cn.hutool.log.Log;
import cn.nis.ntc.utils.bean.LocationPage;
import cn.nis.ntc.utils.exception.NtcException;
import com.baomidou.mybatisplus.annotation.TableField;
import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.csvreader.CsvReader;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.ValidationException;
import org.apache.commons.beanutils.ConvertUtils;
import org.eclipse.collections.impl.list.fixed.ArrayAdapter;

import java.io.File;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.annotation.Annotation;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;
import java.math.BigDecimal;
import java.net.URISyntaxException;
import java.net.URLDecoder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.*;
import java.util.Map.Entry;
import java.util.function.*;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

import static cn.nis.ntc.utils.DPs.log;


/**
 * 高频方法集合类
 */
public class ToolUtil {

    private static final Log logger = Log.get();

    private static final Pattern POINT_PATTERN = Pattern.compile("(\\s)(\\.)(\\s)|(\\.)(\\s)|(\\s)(\\.)");

    private static final Pattern POINT_PATTERN_DEEP = Pattern.compile("(\\.)");

    private static final Pattern humpPattern = Pattern.compile("[A-Z]");

    /**
     * 入库字段\处理查询入库
     *
     * @param cfgKeywords
     * @return
     */
    public static String strGetEscape(String cfgKeywords) {
        if (StringUtils.isNotEmpty(cfgKeywords)) {
            cfgKeywords = cfgKeywords.trim();// 首先去掉空白符号
            // 不能输入不可见字符
            if (containsInvisibleChar(cfgKeywords)) {
                throw new NtcException(Code.InvisibleChar.getCode(), Code.InvisibleChar.getDesc() + ": " + cfgKeywords);
            }
            // \\ 需要第一个替换
//            String[] fbsArr = { "\\","$","(",")","*","+",".","?","^","|","'","%","&"};
            cfgKeywords = cfgKeywords.replace("\\", "\\\\")
                    .replace("&", "\\&")
                    .replace(" ", "\\b");
        }
        return cfgKeywords;
    }

    /**
     * 反转义特殊字符 出库
     *
     * @param cfgKeywords
     * @return
     */
    public static String strUnEscape(String cfgKeywords, Boolean... flag) {
        if (StringUtils.isNotEmpty(cfgKeywords)) {
            // 不转译特殊字符
            cfgKeywords = cfgKeywords.trim();// 首先去掉首尾空格
            if (flag.length > 0 && flag[0]) {
                /**因为入库前空格要转换成了\b;但如果输入的是\b而不是空格，入库后就是\\b，这时在反转时不需要处理，
                 * 在查询的时候要将\b转反换回空格，需要对\\b提前处理，否则会误转
                 * * split本身对\需要转义，最终\\b写成了8个\
                 */
                String[] cfgKeywordAry = cfgKeywords.split("\\\\\\\\b", -1);
//                String tempKeywords = "";
                StringBuilder tempKeywords = new StringBuilder();
                for (String str : cfgKeywordAry) {
                    str = str.replace("\\b", " ");
//                    tempKeywords+=str+"\\\\b";
                    tempKeywords.append(str);
                    tempKeywords.append("\\\\b");
                }
                if (StringUtils.endsWith(tempKeywords.toString(), "\\\\b")) {
                    cfgKeywords = tempKeywords.substring(0, tempKeywords.lastIndexOf("\\\\b"));
                }
                cfgKeywords = cfgKeywords.indexOf("\\\\b") == -1 ? cfgKeywords.replace("\\b", " ") : cfgKeywords;
            } else {
                String[] tempStr = cfgKeywords.split("\\\\\\\\b", -1);
//                String tempKwd  = "";
                StringBuilder tempKwd = new StringBuilder();
                for (String str : tempStr) {
                    str = str.replace("\\b", "\b");
//                    tempKwd += str+"\\\\b";
                    tempKwd.append(str);
                    tempKwd.append("\\\\b");
                }
                if (StringUtils.endsWith(tempKwd.toString(), "\\\\b")) {
                    cfgKeywords = tempKwd.substring(0, tempKwd.lastIndexOf("\\\\b"));
                }
                char[] chars = cfgKeywords.toCharArray();
                StringBuilder sb = new StringBuilder();
                for (char aChar : chars) {
                    if (aChar == '\b') {
                        sb.append(" ");
                    } else {
                        sb.append(aChar);
                    }
                }
                cfgKeywords = sb.toString();

            }
            cfgKeywords = cfgKeywords.replace("\\\\", "\\");
            cfgKeywords = cfgKeywords.replace("\\&", "&");
        }
        return cfgKeywords;
    }

    /**
     * 字符串是否包含不可见字符
     * 包含：true
     * 不包含：false
     *
     * @param content
     * @return Boolean
     */
    public static Boolean containsInvisibleChar(String content) {
        if (content != null && content.length() > 0) {
            char[] contentCharArr = content.toCharArray();
            for (int i = 0; i < contentCharArr.length; i++) {
                if ((contentCharArr[i] <= 0x1F) || contentCharArr[i] == 0x7F) {
                    return true;
                }
            }
            return false;
        }
        return false;
    }
}
